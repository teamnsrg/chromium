// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Bitmap;
import android.graphics.Picture;
import android.net.http.SslError;

import org.chromium.android_webview.AwConsoleMessage;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageCommitVisibleHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnReceivedErrorHelper;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

/**
 * AwContentsClient subclass used for testing.
 */
public class TestAwContentsClient extends NullContentsClient {
    private static final boolean TRACE = false;
    private static final String TAG = "TestAwContentsClient";

    private boolean mAllowSslError;
    private final OnPageStartedHelper mOnPageStartedHelper;
    private final OnPageFinishedHelper mOnPageFinishedHelper;
    private final OnPageCommitVisibleHelper mOnPageCommitVisibleHelper;
    private final OnReceivedErrorHelper mOnReceivedErrorHelper;
    private final OnReceivedError2Helper mOnReceivedError2Helper;
    private final OnReceivedHttpErrorHelper mOnReceivedHttpErrorHelper;
    private final OnReceivedSslErrorHelper mOnReceivedSslErrorHelper;
    private final OnDownloadStartHelper mOnDownloadStartHelper;
    private final OnReceivedLoginRequestHelper mOnReceivedLoginRequestHelper;
    private final OnEvaluateJavaScriptResultHelper mOnEvaluateJavaScriptResultHelper;
    private final AddMessageToConsoleHelper mAddMessageToConsoleHelper;
    private final OnScaleChangedHelper mOnScaleChangedHelper;
    private final OnReceivedTitleHelper mOnReceivedTitleHelper;
    private final PictureListenerHelper mPictureListenerHelper;
    private final ShouldOverrideUrlLoadingHelper mShouldOverrideUrlLoadingHelper;
    private final ShouldInterceptRequestHelper mShouldInterceptRequestHelper;
    private final OnLoadResourceHelper mOnLoadResourceHelper;
    private final DoUpdateVisitedHistoryHelper mDoUpdateVisitedHistoryHelper;
    private final OnCreateWindowHelper mOnCreateWindowHelper;
    private final FaviconHelper mFaviconHelper;
    private final TouchIconHelper mTouchIconHelper;

    public TestAwContentsClient() {
        super(ThreadUtils.getUiThreadLooper());
        mOnPageStartedHelper = new OnPageStartedHelper();
        mOnPageFinishedHelper = new OnPageFinishedHelper();
        mOnPageCommitVisibleHelper = new OnPageCommitVisibleHelper();
        mOnReceivedErrorHelper = new OnReceivedErrorHelper();
        mOnReceivedError2Helper = new OnReceivedError2Helper();
        mOnReceivedHttpErrorHelper = new OnReceivedHttpErrorHelper();
        mOnReceivedSslErrorHelper = new OnReceivedSslErrorHelper();
        mOnDownloadStartHelper = new OnDownloadStartHelper();
        mOnReceivedLoginRequestHelper = new OnReceivedLoginRequestHelper();
        mOnEvaluateJavaScriptResultHelper = new OnEvaluateJavaScriptResultHelper();
        mAddMessageToConsoleHelper = new AddMessageToConsoleHelper();
        mOnScaleChangedHelper = new OnScaleChangedHelper();
        mOnReceivedTitleHelper = new OnReceivedTitleHelper();
        mPictureListenerHelper = new PictureListenerHelper();
        mShouldOverrideUrlLoadingHelper = new ShouldOverrideUrlLoadingHelper();
        mShouldInterceptRequestHelper = new ShouldInterceptRequestHelper();
        mOnLoadResourceHelper = new OnLoadResourceHelper();
        mDoUpdateVisitedHistoryHelper = new DoUpdateVisitedHistoryHelper();
        mOnCreateWindowHelper = new OnCreateWindowHelper();
        mFaviconHelper = new FaviconHelper();
        mTouchIconHelper = new TouchIconHelper();
        mAllowSslError = true;
    }

    public OnPageStartedHelper getOnPageStartedHelper() {
        return mOnPageStartedHelper;
    }

    public OnPageCommitVisibleHelper getOnPageCommitVisibleHelper() {
        return mOnPageCommitVisibleHelper;
    }

    public OnPageFinishedHelper getOnPageFinishedHelper() {
        return mOnPageFinishedHelper;
    }

    public OnReceivedErrorHelper getOnReceivedErrorHelper() {
        return mOnReceivedErrorHelper;
    }

    public OnReceivedError2Helper getOnReceivedError2Helper() {
        return mOnReceivedError2Helper;
    }

    public OnReceivedHttpErrorHelper getOnReceivedHttpErrorHelper() {
        return mOnReceivedHttpErrorHelper;
    }

    public OnReceivedSslErrorHelper getOnReceivedSslErrorHelper() {
        return mOnReceivedSslErrorHelper;
    }

    public OnDownloadStartHelper getOnDownloadStartHelper() {
        return mOnDownloadStartHelper;
    }

    public OnReceivedLoginRequestHelper getOnReceivedLoginRequestHelper() {
        return mOnReceivedLoginRequestHelper;
    }

    public OnEvaluateJavaScriptResultHelper getOnEvaluateJavaScriptResultHelper() {
        return mOnEvaluateJavaScriptResultHelper;
    }

    public ShouldOverrideUrlLoadingHelper getShouldOverrideUrlLoadingHelper() {
        return mShouldOverrideUrlLoadingHelper;
    }

    public ShouldInterceptRequestHelper getShouldInterceptRequestHelper() {
        return mShouldInterceptRequestHelper;
    }

    public OnLoadResourceHelper getOnLoadResourceHelper() {
        return mOnLoadResourceHelper;
    }

    public AddMessageToConsoleHelper getAddMessageToConsoleHelper() {
        return mAddMessageToConsoleHelper;
    }

    public DoUpdateVisitedHistoryHelper getDoUpdateVisitedHistoryHelper() {
        return mDoUpdateVisitedHistoryHelper;
    }

    public OnCreateWindowHelper getOnCreateWindowHelper() {
        return mOnCreateWindowHelper;
    }

    public FaviconHelper getFaviconHelper() {
        return mFaviconHelper;
    }

    public TouchIconHelper getTouchIconHelper() {
        return mTouchIconHelper;
    }

    /**
     * Callback helper for onScaleChangedScaled.
     */
    public static class OnScaleChangedHelper extends CallbackHelper {
        private float mPreviousScale;
        private float mCurrentScale;
        public void notifyCalled(float oldScale, float newScale) {
            mPreviousScale = oldScale;
            mCurrentScale = newScale;
            super.notifyCalled();
        }

        public float getOldScale() {
            return mPreviousScale;
        }

        public float getNewScale() {
            return mCurrentScale;
        }

        public float getLastScaleRatio() {
            assert getCallCount() > 0;
            return mCurrentScale / mPreviousScale;
        }
    }

    public OnScaleChangedHelper getOnScaleChangedHelper() {
        return mOnScaleChangedHelper;
    }

    public PictureListenerHelper getPictureListenerHelper() {
        return mPictureListenerHelper;
    }

    /**
     * Callback helper for onReceivedTitle.
     */
    public static class OnReceivedTitleHelper extends CallbackHelper {
        private String mTitle;

        public void notifyCalled(String title) {
            mTitle = title;
            super.notifyCalled();
        }
        public String getTitle() {
            return mTitle;
        }
    }

    public OnReceivedTitleHelper getOnReceivedTitleHelper() {
        return mOnReceivedTitleHelper;
    }

    @Override
    public void onReceivedTitle(String title) {
        if (TRACE) Log.i(TAG, "onReceivedTitle " + title);
        mOnReceivedTitleHelper.notifyCalled(title);
    }

    public String getUpdatedTitle() {
        return mOnReceivedTitleHelper.getTitle();
    }

    @Override
    public void onPageStarted(String url) {
        if (TRACE) Log.i(TAG, "onPageStarted " + url);
        mOnPageStartedHelper.notifyCalled(url);
    }

    @Override
    public void onPageCommitVisible(String url) {
        if (TRACE) Log.i(TAG, "onPageCommitVisible " + url);
        mOnPageCommitVisibleHelper.notifyCalled(url);
    }

    @Override
    public void onPageFinished(String url) {
        if (TRACE) Log.i(TAG, "onPageFinished " + url);
        mOnPageFinishedHelper.notifyCalled(url);
    }

    @Override
    public void onReceivedError(int errorCode, String description, String failingUrl) {
        if (TRACE) Log.i(TAG, "onReceivedError " + failingUrl);
        mOnReceivedErrorHelper.notifyCalled(errorCode, description, failingUrl);
    }

    @Override
    public void onReceivedError2(AwWebResourceRequest request, AwWebResourceError error) {
        if (TRACE) Log.i(TAG, "onReceivedError2 " + request.url);
        mOnReceivedError2Helper.notifyCalled(request, error);
    }

    @Override
    public void onReceivedSslError(Callback<Boolean> callback, SslError error) {
        if (TRACE) Log.i(TAG, "onReceivedSslError");
        callback.onResult(mAllowSslError);
        mOnReceivedSslErrorHelper.notifyCalled(error);
    }

    public void setAllowSslError(boolean allow) {
        mAllowSslError = allow;
    }

    /**
     * CallbackHelper for OnDownloadStart.
     */
    public static class OnDownloadStartHelper extends CallbackHelper {
        private String mUrl;
        private String mUserAgent;
        private String mContentDisposition;
        private String mMimeType;
        long mContentLength;

        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }

        public String getUserAgent() {
            assert getCallCount() > 0;
            return mUserAgent;
        }

        public String getContentDisposition() {
            assert getCallCount() > 0;
            return mContentDisposition;
        }

        public String getMimeType() {
            assert getCallCount() > 0;
            return mMimeType;
        }

        public long getContentLength() {
            assert getCallCount() > 0;
            return mContentLength;
        }

        public void notifyCalled(String url, String userAgent, String contentDisposition,
                String mimeType, long contentLength) {
            mUrl = url;
            mUserAgent = userAgent;
            mContentDisposition = contentDisposition;
            mMimeType = mimeType;
            mContentLength = contentLength;
            notifyCalled();
        }
    }

    @Override
    public void onDownloadStart(String url,
            String userAgent,
            String contentDisposition,
            String mimeType,
            long contentLength) {
        if (TRACE) Log.i(TAG, "onDownloadStart " + url);
        getOnDownloadStartHelper().notifyCalled(url, userAgent, contentDisposition, mimeType,
                contentLength);
    }

    /**
     * Callback helper for onCreateWindow.
     */
    public static class OnCreateWindowHelper extends CallbackHelper {
        private boolean mIsDialog;
        private boolean mIsUserGesture;
        private boolean mReturnValue;

        public boolean getIsDialog() {
            assert getCallCount() > 0;
            return mIsDialog;
        }

        public boolean getUserAgent() {
            assert getCallCount() > 0;
            return mIsUserGesture;
        }

        public void setReturnValue(boolean returnValue) {
            mReturnValue = returnValue;
        }

        public boolean notifyCalled(boolean isDialog, boolean isUserGesture) {
            mIsDialog = isDialog;
            mIsUserGesture = isUserGesture;
            boolean returnValue = mReturnValue;
            notifyCalled();
            return returnValue;
        }
    }

    @Override
    public boolean onCreateWindow(boolean isDialog, boolean isUserGesture) {
        if (TRACE) Log.i(TAG, "onCreateWindow");
        return mOnCreateWindowHelper.notifyCalled(isDialog, isUserGesture);
    }

    /**
     * CallbackHelper for OnReceivedLoginRequest.
     */
    public static class OnReceivedLoginRequestHelper extends CallbackHelper {
        private String mRealm;
        private String mAccount;
        private String mArgs;

        public String getRealm() {
            assert getCallCount() > 0;
            return mRealm;
        }

        public String getAccount() {
            assert getCallCount() > 0;
            return mAccount;
        }

        public String getArgs() {
            assert getCallCount() > 0;
            return mArgs;
        }

        public void notifyCalled(String realm, String account, String args) {
            mRealm = realm;
            mAccount = account;
            mArgs = args;
            notifyCalled();
        }
    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
        if (TRACE) Log.i(TAG, "onReceivedLoginRequest " + realm);
        getOnReceivedLoginRequestHelper().notifyCalled(realm, account, args);
    }

    @Override
    public boolean onConsoleMessage(AwConsoleMessage consoleMessage) {
        if (TRACE) Log.i(TAG, "onConsoleMessage " + consoleMessage);
        mAddMessageToConsoleHelper.notifyCalled(consoleMessage);
        return false;
    }

    /**
     * Callback helper for AddMessageToConsole.
     */
    public static class AddMessageToConsoleHelper extends CallbackHelper {
        private List<AwConsoleMessage> mMessages = new ArrayList<AwConsoleMessage>();

        public void clearMessages() {
            mMessages.clear();
        }

        public List<AwConsoleMessage> getMessages() {
            return mMessages;
        }

        public int getLevel() {
            assert getCallCount() > 0;
            return getLastMessage().messageLevel();
        }

        public String getMessage() {
            assert getCallCount() > 0;
            return getLastMessage().message();
        }

        public int getLineNumber() {
            assert getCallCount() > 0;
            return getLastMessage().lineNumber();
        }

        public String getSourceId() {
            assert getCallCount() > 0;
            return getLastMessage().sourceId();
        }

        private AwConsoleMessage getLastMessage() {
            return mMessages.get(mMessages.size() - 1);
        }

        void notifyCalled(AwConsoleMessage message) {
            mMessages.add(message);
            notifyCalled();
        }
    }

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
        if (TRACE) Log.i(TAG, "onScaleChangedScaled " + oldScale + " -> " + newScale);
        mOnScaleChangedHelper.notifyCalled(oldScale, newScale);
    }

    /**
     * Callback helper for PictureListener.
     */
    public static class PictureListenerHelper extends CallbackHelper {
        // Generally null, depending on |invalidationOnly| in enableOnNewPicture()
        private Picture mPicture;

        public Picture getPicture() {
            assert getCallCount() > 0;
            return mPicture;
        }

        void notifyCalled(Picture picture) {
            mPicture = picture;
            notifyCalled();
        }
    }

    @Override
    public void onNewPicture(Picture picture) {
        if (TRACE) Log.i(TAG, "onNewPicture");
        mPictureListenerHelper.notifyCalled(picture);
    }

    /**
     * Callback helper for ShouldOverrideUrlLoading.
     */
    public static class ShouldOverrideUrlLoadingHelper extends CallbackHelper {
        private String mShouldOverrideUrlLoadingUrl;
        private boolean mShouldOverrideUrlLoadingReturnValue;
        private boolean mIsRedirect;
        private boolean mHasUserGesture;
        private boolean mIsMainFrame;
        void setShouldOverrideUrlLoadingUrl(String url) {
            mShouldOverrideUrlLoadingUrl = url;
        }
        void setShouldOverrideUrlLoadingReturnValue(boolean value) {
            mShouldOverrideUrlLoadingReturnValue = value;
        }
        public String getShouldOverrideUrlLoadingUrl() {
            assert getCallCount() > 0;
            return mShouldOverrideUrlLoadingUrl;
        }
        public boolean getShouldOverrideUrlLoadingReturnValue() {
            return mShouldOverrideUrlLoadingReturnValue;
        }
        public boolean isRedirect() {
            return mIsRedirect;
        }
        public boolean hasUserGesture() {
            return mHasUserGesture;
        }
        public boolean isMainFrame() {
            return mIsMainFrame;
        }
        public void notifyCalled(
                String url, boolean isRedirect, boolean hasUserGesture, boolean isMainFrame) {
            mShouldOverrideUrlLoadingUrl = url;
            mIsRedirect = isRedirect;
            mHasUserGesture = hasUserGesture;
            mIsMainFrame = isMainFrame;
            notifyCalled();
        }
    }

    @Override
    public boolean shouldOverrideUrlLoading(AwWebResourceRequest request) {
        if (TRACE) Log.i(TAG, "shouldOverrideUrlLoading " + request.url);
        super.shouldOverrideUrlLoading(request);
        boolean returnValue =
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingReturnValue();
        mShouldOverrideUrlLoadingHelper.notifyCalled(
                request.url, request.isRedirect, request.hasUserGesture, request.isMainFrame);
        return returnValue;
    }

    /**
     * Callback helper for shouldInterceptRequest.
     */
    public static class ShouldInterceptRequestHelper extends CallbackHelper {
        private List<String> mShouldInterceptRequestUrls = new ArrayList<String>();
        private ConcurrentHashMap<String, AwWebResourceResponse> mReturnValuesByUrls =
                new ConcurrentHashMap<String, AwWebResourceResponse>();
        private ConcurrentHashMap<String, AwWebResourceRequest> mRequestsByUrls =
                new ConcurrentHashMap<String, AwWebResourceRequest>();
        // This is read on another thread, so needs to be marked volatile.
        private volatile AwWebResourceResponse mShouldInterceptRequestReturnValue;
        void setReturnValue(AwWebResourceResponse value) {
            mShouldInterceptRequestReturnValue = value;
        }
        void setReturnValueForUrl(String url, AwWebResourceResponse value) {
            mReturnValuesByUrls.put(url, value);
        }
        public List<String> getUrls() {
            assert getCallCount() > 0;
            return mShouldInterceptRequestUrls;
        }
        public AwWebResourceResponse getReturnValue(String url) {
            AwWebResourceResponse value = mReturnValuesByUrls.get(url);
            if (value != null) return value;
            return mShouldInterceptRequestReturnValue;
        }
        public AwWebResourceRequest getRequestsForUrl(String url) {
            assert getCallCount() > 0;
            assert mRequestsByUrls.containsKey(url);
            return mRequestsByUrls.get(url);
        }
        public void notifyCalled(AwWebResourceRequest request) {
            mShouldInterceptRequestUrls.add(request.url);
            mRequestsByUrls.put(request.url, request);
            notifyCalled();
        }
    }

    @Override
    public AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request) {
        super.shouldInterceptRequest(request);
        if (TRACE) Log.i(TAG, "shouldInterceptRequest " + request.url);
        AwWebResourceResponse returnValue =
                mShouldInterceptRequestHelper.getReturnValue(request.url);
        mShouldInterceptRequestHelper.notifyCalled(request);
        return returnValue;
    }

    /**
     * Callback helper for OnLoadedResource.
     */
    public static class OnLoadResourceHelper extends CallbackHelper {
        private String mLastLoadedResource;

        public String getLastLoadedResource() {
            assert getCallCount() > 0;
            return mLastLoadedResource;
        }

        public void notifyCalled(String url) {
            mLastLoadedResource = url;
            notifyCalled();
        }
    }

    @Override
    public void onLoadResource(String url) {
        if (TRACE) Log.i(TAG, "onLoadResource " + url);
        super.onLoadResource(url);
        mOnLoadResourceHelper.notifyCalled(url);
    }

    /**
     * Callback helper for doUpdateVisitedHistory.
     */
    public static class DoUpdateVisitedHistoryHelper extends CallbackHelper {
        String mUrl;
        boolean mIsReload;

        public String getUrl() {
            assert getCallCount() > 0;
            return mUrl;
        }

        public boolean getIsReload() {
            assert getCallCount() > 0;
            return mIsReload;
        }

        public void notifyCalled(String url, boolean isReload) {
            mUrl = url;
            mIsReload = isReload;
            notifyCalled();
        }
    }

    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
        if (TRACE) Log.i(TAG, "doUpdateVisitedHistory " + url);
        getDoUpdateVisitedHistoryHelper().notifyCalled(url, isReload);
    }

    /**
     * CallbackHelper for onReceivedSslError.
     */
    public static class OnReceivedSslErrorHelper extends CallbackHelper {
        private SslError mSslError;

        public void notifyCalled(SslError error) {
            mSslError = error;
            notifyCalled();
        }

        public SslError getError() {
            assert getCallCount() > 0;
            return mSslError;
        }
    }

    /**
     * CallbackHelper for OnReceivedError2.
     */
    public static class OnReceivedError2Helper extends CallbackHelper {
        private AwWebResourceRequest mRequest;
        private AwWebResourceError mError;
        public void notifyCalled(AwWebResourceRequest request, AwWebResourceError error) {
            mRequest = request;
            mError = error;
            notifyCalled();
        }
        public AwWebResourceRequest getRequest() {
            assert getCallCount() > 0;
            return mRequest;
        }
        public AwWebResourceError getError() {
            assert getCallCount() > 0;
            return mError;
        }
    }

    /**
     * CallbackHelper for OnReceivedHttpError.
     */
    public static class OnReceivedHttpErrorHelper extends CallbackHelper {
        private AwWebResourceRequest mRequest;
        private AwWebResourceResponse mResponse;

        public void notifyCalled(AwWebResourceRequest request, AwWebResourceResponse response) {
            mRequest = request;
            mResponse = response;
            notifyCalled();
        }
        public AwWebResourceRequest getRequest() {
            assert getCallCount() > 0;
            return mRequest;
        }
        public AwWebResourceResponse getResponse() {
            assert getCallCount() > 0;
            return mResponse;
        }
    }

    @Override
    public void onReceivedHttpError(AwWebResourceRequest request, AwWebResourceResponse response) {
        if (TRACE) Log.i(TAG, "onReceivedHttpError " + request.url);
        super.onReceivedHttpError(request, response);
        mOnReceivedHttpErrorHelper.notifyCalled(request, response);
    }

    /**
     * CallbackHelper for onReceivedIcon.
     */
    public static class FaviconHelper extends CallbackHelper {
        private Bitmap mIcon;

        public void notifyFavicon(Bitmap icon) {
            mIcon = icon;
            super.notifyCalled();
        }

        public Bitmap getIcon() {
            assert getCallCount() > 0;
            return mIcon;
        }
    }

    @Override
    public void onReceivedIcon(Bitmap bitmap) {
        if (TRACE) Log.i(TAG, "onReceivedIcon");
        // We don't inform the API client about the URL of the icon.
        mFaviconHelper.notifyFavicon(bitmap);
    }

    /**
     * CallbackHelper for onReceivedTouchIconUrl.
     */
    public static class TouchIconHelper extends CallbackHelper {
        private HashMap<String, Boolean> mTouchIcons = new HashMap<String, Boolean>();

        public void notifyTouchIcon(String url, boolean precomposed) {
            mTouchIcons.put(url, precomposed);
            super.notifyCalled();
        }

        public int getTouchIconsCount() {
            assert getCallCount() > 0;
            return mTouchIcons.size();
        }

        public boolean hasTouchIcon(String url) {
            return mTouchIcons.get(url);
        }
    }

    @Override
    public void onReceivedTouchIconUrl(String url, boolean precomposed) {
        if (TRACE) Log.i(TAG, "onReceivedTouchIconUrl " + url);
        mTouchIconHelper.notifyTouchIcon(url, precomposed);
    }
}
