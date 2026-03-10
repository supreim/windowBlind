import { NextResponse } from "next/server";

type BlindState = {
  motorPosition: number; // 0 to 100
  uvs: number;
  als: number;
  autoMode: boolean;
  windowLevel: number; // desired blind position 0 to 100
  updatedAt: string;
};

const globalForBlinds = globalThis as typeof globalThis & {
  blindState?: BlindState;
};

const defaultState: BlindState = {
  motorPosition: 0,
  uvs: 0,
  als: 0,
  autoMode: false,
  windowLevel: 0,
  updatedAt: new Date().toISOString(),
};

const blindState = globalForBlinds.blindState ?? defaultState;
globalForBlinds.blindState = blindState;

const clampPercent = (value: unknown) => {
  const num = Number(value);
  if (!Number.isFinite(num)) return 0;
  return Math.max(0, Math.min(100, num));
};

const toNumber = (value: unknown, fallback = 0) => {
  const num = Number(value);
  return Number.isFinite(num) ? num : fallback;
};

const toBoolean = (value: unknown, fallback = false) => {
  if (typeof value === "boolean") return value;
  if (typeof value === "string") {
    const v = value.trim().toLowerCase();
    if (v === "true" || v === "1" || v === "on") return true;
    if (v === "false" || v === "0" || v === "off") return false;
  }
  if (typeof value === "number") return value !== 0;
  return fallback;
};

// ESP32 sends sensor and motor data here
export async function POST(request: Request) {
  try {
    const body = await request.json();

    if ("motorPosition" in body) {
      blindState.motorPosition = clampPercent(body.motorPosition);
    }

    if ("uvs" in body) {
      blindState.uvs = toNumber(body.uvs, blindState.uvs);
    }

    if ("als" in body) {
      blindState.als = toNumber(body.als, blindState.als);
    }

    // Optional: allow website to also update these through POST if needed
    if ("autoMode" in body) {
      blindState.autoMode = toBoolean(body.autoMode, blindState.autoMode);
    }

    if ("windowLevel" in body) {
      blindState.windowLevel = clampPercent(body.windowLevel);
    }

    blindState.updatedAt = new Date().toISOString();

    return NextResponse.json({
      ok: true,
      message: "State updated",
      state: blindState,
    });
  } catch {
    return NextResponse.json(
      {
        ok: false,
        message: "Invalid JSON body",
      },
      { status: 400 }
    );
  }
}

// ESP32 reads autoMode and desired window level from here
export async function GET() {
  return NextResponse.json({
    ok: true,
    autoMode: blindState.autoMode,
    windowLevel: blindState.windowLevel,
    updatedAt: blindState.updatedAt,
  });
}