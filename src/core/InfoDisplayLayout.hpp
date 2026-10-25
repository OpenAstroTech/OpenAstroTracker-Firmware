#pragma once

namespace core
{

/// The geometry of the 128x64 status panel's two scales.
///
/// The numbers differ per product (the OAM/OAE firmware maps the whole DEC circle, the
/// tracker half of it), so they are passed in rather than compiled in: the adapter knows its
/// panel, this file only knows the arithmetic.
struct InfoDisplayScale {
    /// Pixel positions of the RA scale's left edge, its width, and the dotted line it sits on.
    int leftEdgeMount;
    int raSize;
    int raScalePos;

    /// Pixel positions of the DEC scale: its top, its height and the dotted line.
    int topEdgeMount;
    int decSize;
    int decScalePos;

    /// RA hours at the left and right edge of the scale.
    float leftRa;
    float rightRa;

    /// DEC degrees at the bottom edge and the span the scale covers.
    float bottomDec;
    float rangeDec;
};

/// Pixel column for an RA hour angle on the scale.
int xRAPixel(const InfoDisplayScale &scale, float raHours);

/// Pixel row for a DEC angle on the scale.
int yDECPixel(const InfoDisplayScale &scale, float decDegrees);

/// Width in pixels of a slew-progress bar segment, 0..100 percent.
int progressBarWidth(int percent);

/// RA hours a position marker sits at, from the tracking and slew step counters.
float markerRaHours(long trackingSteps, long slewSteps, float raStepsPerDegree, int trackingMicrosteps, int slewMicrosteps);

/// DEC degrees a position marker sits at, from the slew step counter.
float markerDecDegrees(long decSteps, float decStepsPerDegree);

}  // namespace core
