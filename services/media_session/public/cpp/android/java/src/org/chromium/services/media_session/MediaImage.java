// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.media_session;

import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.text.TextUtils;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.List;

/**
 * The MediaImage class carries the artwork information in MediaMetadata. It is the Java
 * counterpart of media_session::MediaImage.
 */
@JNINamespace("media_session")
public final class MediaImage {
    @NonNull
    private String mSrc;

    private String mType;

    @NonNull
    private List<Rect> mSizes = new ArrayList<Rect>();

    /**
     * Creates a new MediaImage.
     */
    public MediaImage(@NonNull String src, @NonNull String type, @NonNull List<Rect> sizes) {
        mSrc = src;
        mType = type;
        mSizes = sizes;
    }

    /**
     * @return The URL of this MediaImage.
     */
    @NonNull
    public String getSrc() {
        return mSrc;
    }

    /**
     * @return The MIME type of this MediaImage.
     */
    public String getType() {
        return mType;
    }

    /**
     * @return The hinted sizes of this MediaImage.
     */
    public List<Rect> getSizes() {
        return mSizes;
    }

    /**
     * Sets the URL of this MediaImage.
     */
    public void setSrc(@NonNull String src) {
        mSrc = src;
    }

    /**
     * Sets the MIME type of this MediaImage.
     */
    public void setType(@NonNull String type) {
        mType = type;
    }

    /**
     * Sets the sizes of this MediaImage.
     */
    public void setSizes(@NonNull List<Rect> sizes) {
        mSizes = sizes;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) return true;
        if (!(obj instanceof MediaImage)) return false;

        MediaImage other = (MediaImage) obj;
        return TextUtils.equals(mSrc, other.mSrc) && TextUtils.equals(mType, other.mType)
                && mSizes.equals(other.mSizes);
    }

    /**
     * @return The hash code of this {@link MediaImage}. The method uses the same algorithm in
     * {@link java.util.List} for combinine hash values.
     */
    @Override
    public int hashCode() {
        int result = mSrc.hashCode();
        result = 31 * result + mType.hashCode();
        result = 31 * result + mSizes.hashCode();
        return result;
    }

    /**
     * Create a new {@link MediaImage} from the C++ code.
     * @param src The URL of the image.
     * @param type The MIME type of the image.
     * @param flattenedSizes The flattened array of image sizes. In native code, it is of type
     *         `std::vector<gfx::Size>` before flattening.
     */
    @CalledByNative
    private static MediaImage create(String src, String type, int[] flattenedSizes) {
        assert (flattenedSizes.length % 2) == 0;
        List<Rect> sizes = new ArrayList<Rect>();
        for (int i = 0; (i + 1) < flattenedSizes.length; i += 2) {
            sizes.add(new Rect(0, 0, flattenedSizes[i], flattenedSizes[i + 1]));
        }
        return new MediaImage(src, type, sizes);
    }
}
