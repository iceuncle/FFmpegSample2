package com.sample.ffmpegdemo.utils;

import android.view.Surface;

/**
 * 界面描述：
 * <p>
 * Created by tianyang on 2020/12/13.
 */
public class FFUtils {

    public static native String urlProtocolInfo();

    public static native String avFormatInfo();

    public static native String avCodecInfo();

    public static native String avFilterInfo();

    public static native void playVideo(String videoPath, Surface surface);
}
