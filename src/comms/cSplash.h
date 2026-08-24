#pragma once

//-----------------------------------------------------------------------------
// cSplash
//-----------------------------------------------------------------------------
class cSplash {
public:
	struct ShowArgs {
		cStr Version;
		int OffsetX, OffsetY;
        float DelaySec;
        int CornerRadius;
		ShowArgs() {
			SetDefaults();
		}
		void SetDefaults() {
			Version = "0.0.0";
			OffsetX = 0;
			OffsetY = 0;
            DelaySec = 0.0f;
            CornerRadius = 0;
		}
	};
	static void Show(const ShowArgs &);
	static void DestroyNow();
	static void DestroyLater();
}; // cSplash
