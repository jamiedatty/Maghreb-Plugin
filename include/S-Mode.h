#pragma once
#include <windows.h>
#include <cstring>
#include "EuroScopePlugIn.h"

namespace SMode
{
    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    const int  kIndicatorVerticalOffset   = 4;
    const int  kTrackColorSampleRadius    = 2;
    const int  kMaximumGroundSpeedKnots   = 80;
    const char kSModeIndicatorText[]      = "S";

    // Sentinel meaning "not yet captured"
    const COLORREF kColorNotCaptured = CLR_INVALID;

    // -------------------------------------------------------------------------
    // Persistent color state — captured once and held for the plugin lifetime
    // -------------------------------------------------------------------------
    inline COLORREF& GetUnassumedColor()
    {
        static COLORREF s_color = kColorNotCaptured;
        return s_color;
    }

    inline COLORREF& GetAssumedColor()
    {
        static COLORREF s_color = kColorNotCaptured;
        return s_color;
    }

    // -------------------------------------------------------------------------
    // Helper: sample the rendered track color from the HDC
    // -------------------------------------------------------------------------
    inline bool TryGetRenderedTrackColor(HDC hDC, const POINT& trackPoint, COLORREF* color)
    {
        const COLORREF background =
            GetPixel(hDC,
                     trackPoint.x + kTrackColorSampleRadius + 1,
                     trackPoint.y + kTrackColorSampleRadius + 3);

        for (int radius = 0; radius <= kTrackColorSampleRadius; ++radius) {
            for (int y = trackPoint.y - radius; y <= trackPoint.y + radius; ++y) {
                for (int x = trackPoint.x - radius; x <= trackPoint.x + radius; ++x) {
                    // Only walk the perimeter of each expanding square
                    if (x != trackPoint.x - radius && x != trackPoint.x + radius &&
                        y != trackPoint.y - radius && y != trackPoint.y + radius)
                        continue;

                    const COLORREF sample = GetPixel(hDC, x, y);
                    if (sample != CLR_INVALID &&
                        sample != GetBkColor(hDC) &&
                        sample != background)
                    {
                        *color = sample;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Target classification helpers
    // -------------------------------------------------------------------------
    inline bool HasModeC(const EuroScopePlugIn::CRadarTarget& target)
    {
        const EuroScopePlugIn::CRadarTargetPositionData pos = target.GetPosition();
        return pos.IsValid() && pos.GetTransponderC();
    }

    inline bool IsKnownGroundState(const char* groundState)
    {
        return groundState != nullptr &&
               (lstrcmpiA(groundState, "STUP")   == 0 ||
                lstrcmpiA(groundState, "PUSH")   == 0 ||
                lstrcmpiA(groundState, "TAXI")   == 0 ||
                lstrcmpiA(groundState, "DEPA")   == 0 ||
                lstrcmpiA(groundState, "ARR")    == 0 ||
                lstrcmpiA(groundState, "TAXIIN") == 0 ||
                lstrcmpiA(groundState, "PARK")   == 0);
    }

    inline bool IsOnGround(EuroScopePlugIn::CPlugIn* plugin,
                           const EuroScopePlugIn::CRadarTarget& target)
    {
        EuroScopePlugIn::CFlightPlan fp = target.GetCorrelatedFlightPlan();
        if (!fp.IsValid() && plugin != nullptr)
            fp = plugin->FlightPlanSelect(target.GetCallsign());
        return fp.IsValid() && IsKnownGroundState(fp.GetGroundState());
    }

    inline bool LooksLikeGroundTraffic(const EuroScopePlugIn::CRadarTarget& target)
    {
        return target.GetGS() <= kMaximumGroundSpeedKnots;
    }

    // Returns true if this target is assumed by the local controller
    inline bool IsAssumed(const EuroScopePlugIn::CRadarTarget& target)
    {
        const EuroScopePlugIn::CFlightPlan fp = target.GetCorrelatedFlightPlan();
        if (!fp.IsValid())
            return false;
        return fp.GetState() == EuroScopePlugIn::FLIGHT_PLAN_STATE_ASSUMED;
    }

    // -------------------------------------------------------------------------
    // Core drawing — captures colors on first opportunity, then reuses them
    // -------------------------------------------------------------------------
    inline void DrawIndicator(HDC hDC,
                              EuroScopePlugIn::CPlugIn*      plugin,
                              EuroScopePlugIn::CRadarTarget& target,
                              EuroScopePlugIn::CRadarScreen* screen)
    {
        if (!HasModeC(target))
            return;

        const EuroScopePlugIn::CRadarTargetPositionData position = target.GetPosition();
        if (IsOnGround(plugin, target) || LooksLikeGroundTraffic(target))
            return;

        const POINT trackPoint =
            screen->ConvertCoordFromPositionToPixel(position.GetPosition());

        const bool assumed = IsAssumed(target);

        COLORREF& persistentColor = assumed ? GetAssumedColor() : GetUnassumedColor();

        // If we haven't locked in this slot's color yet, try to sample it now
        if (persistentColor == kColorNotCaptured)
        {
            COLORREF sampled = kColorNotCaptured;
            if (TryGetRenderedTrackColor(hDC, trackPoint, &sampled))
                persistentColor = sampled;          // saved for the rest of the session
        }

        // Decide what color to actually paint with:
        //   - use the captured persistent color if available
        //   - fall back to the HDC text color only if capture hasn't happened yet
        const COLORREF drawColor =
            (persistentColor != kColorNotCaptured) ? persistentColor : GetTextColor(hDC);

        // Build font
        HFONT font = CreateFontA(
            -12, 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            "./EuroScope.tff");

        HFONT       oldFont    = font ? static_cast<HFONT>(SelectObject(hDC, font)) : nullptr;
        COLORREF    oldColor   = SetTextColor(hDC, drawColor);
        int         oldBkMode  = SetBkMode(hDC, TRANSPARENT);

        SIZE textSize = {};
        GetTextExtentPoint32A(hDC,
                              kSModeIndicatorText,
                              static_cast<int>(std::strlen(kSModeIndicatorText)),
                              &textSize);

        TextOutA(hDC,
                 trackPoint.x - (textSize.cx / 2),
                 trackPoint.y + kIndicatorVerticalOffset,
                 kSModeIndicatorText,
                 static_cast<int>(std::strlen(kSModeIndicatorText)));

        SetBkMode(hDC, oldBkMode);
        SetTextColor(hDC, oldColor);
        if (oldFont) SelectObject(hDC, oldFont);
        if (font)    DeleteObject(font);
    }

    // -------------------------------------------------------------------------
    // Iterate every radar target and draw its indicator
    // -------------------------------------------------------------------------
    inline void DrawAllIndicators(HDC hDC,
                                  EuroScopePlugIn::CPlugIn*      plugin,
                                  EuroScopePlugIn::CRadarScreen* screen)
    {
        for (EuroScopePlugIn::CRadarTarget target = plugin->RadarTargetSelectFirst();
             target.IsValid();
             target = plugin->RadarTargetSelectNext(target))
        {
            DrawIndicator(hDC, plugin, target, screen);
        }
    }

} // namespace SMode