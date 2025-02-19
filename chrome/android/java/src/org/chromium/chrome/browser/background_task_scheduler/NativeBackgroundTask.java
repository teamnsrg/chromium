// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_task_scheduler;

import android.content.Context;
import android.support.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerExternalUma;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base class implementing {@link BackgroundTask} that adds native initialization, ensuring that
 * tasks are run after Chrome is successfully started.
 */
public abstract class NativeBackgroundTask implements BackgroundTask {
    private static final String TAG = "BTS_NativeBkgrdTask";

    /** Specifies which action to take following onStartTaskBeforeNativeLoaded. */
    @IntDef({StartBeforeNativeResult.LOAD_NATIVE, StartBeforeNativeResult.RESCHEDULE,
            StartBeforeNativeResult.DONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StartBeforeNativeResult {
        /** Task should continue to load native parts of browser. */
        int LOAD_NATIVE = 0;
        /** Task should request rescheduling, without loading native parts of browser. */
        int RESCHEDULE = 1;
        /** Task should neither load native parts of browser nor reschedule. */
        int DONE = 2;
    }

    protected NativeBackgroundTask() {}

    /** Indicates that the task has already been stopped. Should only be accessed on UI Thread. */
    private boolean mTaskStopped;

    /** The id of the task from {@link TaskParameters} used for metrics logging. */
    private int mTaskId;

    @Override
    public final boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        ThreadUtils.assertOnUiThread();
        mTaskId = taskParameters.getTaskId();

        TaskFinishedCallback wrappedCallback = needsReschedule -> {
            BackgroundTaskSchedulerExternalUma.reportNativeTaskFinished(mTaskId);
            callback.taskFinished(needsReschedule);
        };

        // WrappedCallback will only be called when the work is done or in onStopTask. If the task
        // is short-circuited early (by returning DONE or RESCHEDULE as a StartBeforeNativeResult),
        // the wrappedCallback is not called. Thus task-finished metrics are only recorded if
        // task-started metrics are.
        @StartBeforeNativeResult
        int beforeNativeResult =
                onStartTaskBeforeNativeLoaded(context, taskParameters, wrappedCallback);

        if (beforeNativeResult == StartBeforeNativeResult.DONE) return false;

        if (beforeNativeResult == StartBeforeNativeResult.RESCHEDULE) {
            // Do not pass in wrappedCallback because this is a short-circuit reschedule. For UMA
            // purposes, tasks are started when runWithNative is called and does not consider
            // short-circuit reschedules such as this.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, buildRescheduleRunnable(callback));
            return true;
        }

        BackgroundTaskSchedulerExternalUma.reportNativeTaskStarted(mTaskId);

        assert beforeNativeResult == StartBeforeNativeResult.LOAD_NATIVE;
        runWithNative(context,
                buildStartWithNativeRunnable(context, taskParameters, wrappedCallback),
                buildRescheduleRunnable(wrappedCallback));
        return true;
    }

    @Override
    public final boolean onStopTask(Context context, TaskParameters taskParameters) {
        ThreadUtils.assertOnUiThread();
        mTaskStopped = true;
        BackgroundTaskSchedulerExternalUma.reportNativeTaskFinished(mTaskId);
        if (isNativeLoaded()) {
            return onStopTaskWithNative(context, taskParameters);
        } else {
            return onStopTaskBeforeNativeLoaded(context, taskParameters);
        }
    }

    /**
     * Ensure that native is started before running the task. If native fails to start, the task is
     * going to be rescheduled, by issuing a {@see TaskFinishedCallback} with parameter set to
     * <c>true</c>.
     *
     * @param context the current context
     * @param startWithNativeRunnable A runnable that will execute #onStartTaskWithNative, after the
     *    native is loaded.
     * @param rescheduleRunnable A runnable that will be called to reschedule the task in case
     *    native initialization fails.
     */
    protected final void runWithNative(final Context context,
            final Runnable startWithNativeRunnable, final Runnable rescheduleRunnable) {
        if (isNativeLoaded()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, startWithNativeRunnable);
            return;
        }

        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, startWithNativeRunnable);
            }
            @Override
            public boolean startServiceManagerOnly() {
                return supportsServiceManagerOnly();
            }
            @Override
            public void onStartupFailure() {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, rescheduleRunnable);
            }
        };

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                // If task was stopped before we got here, don't start native initialization.
                if (mTaskStopped) return;

                BackgroundTaskSchedulerExternalUma.reportTaskStartedNative(mTaskId);
                try {
                    ChromeBrowserInitializer.getInstance(context).handlePreNativeStartup(parts);

                    ChromeBrowserInitializer.getInstance(context).handlePostNativeStartup(
                            true /* isAsync */, parts);
                } catch (ProcessInitException e) {
                    Log.e(TAG, "ProcessInitException while starting the browser process.");
                    rescheduleRunnable.run();
                    return;
                }
            }
        });
    }

    /**
     * Descendant classes should override this method if they support running in service manager
     * only mode.
     *
     * TODO(https://crbug.com/913480): implement in a subclass once it can support running in
     * ServiceManager only mode.
     *
     * @return if the task supports running in service manager only mode.
     */
    protected boolean supportsServiceManagerOnly() {
        return false;
    }

    /**
     * Method that should be implemented in derived classes to provide implementation of {@link
     * BackgroundTask#onStartTask(Context, TaskParameters, TaskFinishedCallback)} run before native
     * is loaded. Task implementing the method may decide to not load native if it hasn't been
     * loaded yet, by returning DONE, meaning no more work is required for the task, or RESCHEDULE,
     * meaning task needs to be immediately rescheduled.
     * This method is guaranteed to be called before {@link #onStartTaskWithNative}.
     */
    @StartBeforeNativeResult
    protected abstract int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback);

    /**
     * Method that should be implemented in derived classes to provide implementation of {@link
     * BackgroundTask#onStartTask(Context, TaskParameters, TaskFinishedCallback)} when native is
     * loaded.
     * This method will not be called unless {@link #onStartTaskBeforeNativeLoaded} returns
     * LOAD_NATIVE.
     */
    protected abstract void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback);

    /** Called by {@link #onStopTask} if native part of browser was not loaded. */
    protected abstract boolean onStopTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters);

    /** Called by {@link #onStopTask} if native part of browser was loaded. */
    protected abstract boolean onStopTaskWithNative(Context context, TaskParameters taskParameters);

    /** Builds a runnable rescheduling task. */
    private Runnable buildRescheduleRunnable(final TaskFinishedCallback callback) {
        return new Runnable() {
            @Override
            public void run() {
                ThreadUtils.assertOnUiThread();
                if (mTaskStopped) return;
                callback.taskFinished(true);
            }
        };
    }

    /** Builds a runnable starting task with native portion. */
    private Runnable buildStartWithNativeRunnable(final Context context,
            final TaskParameters taskParameters, final TaskFinishedCallback callback) {
        return new Runnable() {
            @Override
            public void run() {
                ThreadUtils.assertOnUiThread();
                if (mTaskStopped) return;
                onStartTaskWithNative(context, taskParameters, callback);
            }
        };
    }

    /** Whether the native part of the browser is loaded. */
    private boolean isNativeLoaded() {
        return getBrowserStartupController().isStartupSuccessfullyCompleted();
    }

    @VisibleForTesting
    protected BrowserStartupController getBrowserStartupController() {
        return BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER);
    }
}
