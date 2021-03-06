// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.view.Choreographer;
import android.view.WindowManager;

import org.chromium.base.TraceEvent;

/**
 * Notifies clients of the default displays's vertical sync pulses.
 * On ICS, VSyncMonitor relies on setVSyncPointForICS() being called to set a reasonable
 * approximation of a vertical sync starting point; see also http://crbug.com/156397.
 */
@SuppressLint("NewApi")
public class VSyncMonitor {
    private static final long NANOSECONDS_PER_SECOND = 1000000000;
    private static final long NANOSECONDS_PER_MILLISECOND = 1000000;
    private static final long NANOSECONDS_PER_MICROSECOND = 1000;

    /**
     * VSync listener class
     */
    public interface Listener {
        /**
         * Called very soon after the start of the display's vertical sync period.
         * @param monitor The VSyncMonitor that triggered the signal.
         * @param vsyncTimeMicros Absolute frame time in microseconds.
         */
        public void onVSync(VSyncMonitor monitor, long vsyncTimeMicros);
    }

    private Listener mListener;

    // Display refresh rate as reported by the system.
    private final long mRefreshPeriodNano;

    private boolean mHaveRequestInFlight;

    // Choreographer is used to detect vsync on >= JB.
    private final Choreographer mChoreographer;
    private final Choreographer.FrameCallback mVSyncFrameCallback;

    // On ICS we just post a task through the handler (http://crbug.com/156397)
    private final Runnable mVSyncRunnableCallback;
    private long mGoodStartingPointNano;
    private long mLastPostedNano;

    // If the monitor is activated after having been idle, we synthesize the first vsync to reduce
    // latency.
    private final Handler mHandler = new Handler();
    private final Runnable mSyntheticVSyncRunnable;
    private long mLastVSyncCpuTimeNano;

    /**
     * Constructs a VSyncMonitor
     * @param context The application context.
     * @param listener The listener receiving VSync notifications.
     */
    public VSyncMonitor(Context context, VSyncMonitor.Listener listener) {
        this(context, listener, true);
    }

    /**
     * Constructs a VSyncMonitor
     * @param context The application context.
     * @param listener The listener receiving VSync notifications.
     * @param enableJBVsync Whether to allow Choreographer-based notifications on JB and up.
     */
    public VSyncMonitor(Context context, VSyncMonitor.Listener listener, boolean enableJBVSync) {
        mListener = listener;
        float refreshRate = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE))
                .getDefaultDisplay().getRefreshRate();
        if (refreshRate <= 0) refreshRate = 60;
        mRefreshPeriodNano = (long) (NANOSECONDS_PER_SECOND / refreshRate);

        if (enableJBVSync && Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            // Use Choreographer on JB+ to get notified of vsync.
            mChoreographer = Choreographer.getInstance();
            mVSyncFrameCallback = new Choreographer.FrameCallback() {
                @Override
                public void doFrame(long frameTimeNanos) {
                    TraceEvent.begin("VSync");
                    mGoodStartingPointNano = frameTimeNanos;
                    onVSyncCallback(frameTimeNanos, getCurrentNanoTime());
                    TraceEvent.end("VSync");
                }
            };
            mVSyncRunnableCallback = null;
        } else {
            // On ICS we just hope that running tasks is relatively predictable.
            mChoreographer = null;
            mVSyncFrameCallback = null;
            mVSyncRunnableCallback = new Runnable() {
                @Override
                public void run() {
                    TraceEvent.begin("VSyncTimer");
                    final long currentTime = getCurrentNanoTime();
                    onVSyncCallback(currentTime, currentTime);
                    TraceEvent.end("VSyncTimer");
                }
            };
            mLastPostedNano = 0;
        }
        mSyntheticVSyncRunnable = new Runnable() {
            @Override
            public void run() {
                TraceEvent.begin("VSyncSynthetic");
                final long currentTime = getCurrentNanoTime();
                onVSyncCallback(estimateLastVSyncTime(currentTime), currentTime);
                TraceEvent.end("VSyncSynthetic");
            }
        };
        mGoodStartingPointNano = getCurrentNanoTime();
    }

    /**
     * Returns the time interval between two consecutive vsync pulses in microseconds.
     */
    public long getVSyncPeriodInMicroseconds() {
        return mRefreshPeriodNano / NANOSECONDS_PER_MICROSECOND;
    }

    /**
     * Determine whether a true vsync signal is available on this platform.
     */
    private boolean isVSyncSignalAvailable() {
        return mChoreographer != null;
    }

    /**
     * Request to be notified of the closest display vsync events.
     * Listener.onVSync() will be called soon after the upcoming vsync pulses.
     */
    public void requestUpdate() {
        postCallback();
    }

    /**
     * Set the best guess of the point in the past when the vsync has happened.
     * @param goodStartingPointNano Known vsync point in the past.
     */
    public void setVSyncPointForICS(long goodStartingPointNano) {
        mGoodStartingPointNano = goodStartingPointNano;
    }

    private long getCurrentNanoTime() {
        return System.nanoTime();
    }

    private void onVSyncCallback(long frameTimeNanos, long currentTimeNanos) {
        assert mHaveRequestInFlight;
        mHaveRequestInFlight = false;
        mLastVSyncCpuTimeNano = currentTimeNanos;
        if (mListener != null) {
            mListener.onVSync(this, frameTimeNanos / NANOSECONDS_PER_MICROSECOND);
        }
    }

    private void postCallback() {
        if (mHaveRequestInFlight) return;
        mHaveRequestInFlight = true;
        if (postSyntheticVSync()) return;
        if (isVSyncSignalAvailable()) {
            mChoreographer.postFrameCallback(mVSyncFrameCallback);
        } else {
            postRunnableCallback();
        }
    }

    private boolean postSyntheticVSync() {
        final long currentTime = getCurrentNanoTime();
        // Only trigger a synthetic vsync if we've been idle for long enough and the upcoming real
        // vsync is more than half a frame away.
        if (currentTime - mLastVSyncCpuTimeNano < 2 * mRefreshPeriodNano) return false;
        if (currentTime - estimateLastVSyncTime(currentTime) > mRefreshPeriodNano / 2) return false;
        mHandler.post(mSyntheticVSyncRunnable);
        return true;
    }

    private long estimateLastVSyncTime(long currentTime) {
        final long lastRefreshTime = mGoodStartingPointNano +
                ((currentTime - mGoodStartingPointNano) / mRefreshPeriodNano) * mRefreshPeriodNano;
        return lastRefreshTime;
    }

    private void postRunnableCallback() {
        assert !isVSyncSignalAvailable();
        final long currentTime = getCurrentNanoTime();
        final long lastRefreshTime = estimateLastVSyncTime(currentTime);
        long delay = (lastRefreshTime + mRefreshPeriodNano) - currentTime;
        assert delay > 0 && delay <= mRefreshPeriodNano;

        if (currentTime + delay <= mLastPostedNano + mRefreshPeriodNano / 2) {
            delay += mRefreshPeriodNano;
        }

        mLastPostedNano = currentTime + delay;
        if (delay == 0) mHandler.post(mVSyncRunnableCallback);
        else mHandler.postDelayed(mVSyncRunnableCallback, delay / NANOSECONDS_PER_MILLISECOND);
    }
}
