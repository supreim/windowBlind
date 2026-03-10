"use client";

import { useMemo, useState, useEffect } from "react";
import { motion } from "framer-motion";
import { Sun, ArrowUp, ArrowDown, Maximize2, Minimize2, Sparkles, SlidersHorizontal } from "lucide-react";

export default function GestureBlindDashboard() {
  const [blindLevel, setBlindLevel] = useState(55); // 0 = closed, 100 = open
  const [targetLight, setTargetLight] = useState(65);
  const [autoMode, setAutoMode] = useState(true);

  const clamp = (value: number, min: number, max: number) => Math.min(Math.max(value, min), max);

  const increment = () => setBlindLevel((prev) => clamp(prev + 10, 0, 100));
  const decrement = () => setBlindLevel((prev) => clamp(prev - 10, 0, 100));
  const openAll = () => setBlindLevel(100);
  const closeAll = () => setBlindLevel(0);

  const slatCount = 9;
  const closedCoverage = 100 - blindLevel;

  const roomGlow = useMemo(() => {
    const value = blindLevel * 0.8 + targetLight * 0.2;
    return clamp(value, 10, 100);
  }, [blindLevel, targetLight]);

  useEffect(() => {
    if (!autoMode) return;
    const difference = targetLight - blindLevel;
    if (Math.abs(difference) < 2) return;

    const timer = setTimeout(() => {
      setBlindLevel((prev) => {
        const step = difference > 0 ? 2 : -2;
        return clamp(prev + step, 0, 100);
      });
    }, 300);

    return () => clearTimeout(timer);
  }, [targetLight, blindLevel, autoMode]);

  return (
    <div className="min-h-screen bg-neutral-950 text-white px-4 py-8 sm:px-6 lg:px-8">
      <div className="mx-auto grid max-w-7xl gap-6 lg:grid-cols-[1.2fr_0.8fr]">
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ duration: 0.5 }}
          className="rounded-3xl border border-white/10 bg-white/5 p-5 shadow-2xl backdrop-blur-xl sm:p-7"
        >
          <div className="mb-6 flex flex-col gap-4 sm:flex-row sm:items-center sm:justify-between">
            <div>
              <div className="mb-2 inline-flex items-center gap-2 rounded-full border border-white/10 bg-white/5 px-3 py-1 text-xs text-white/70">
                <Sparkles className="h-4 w-4" />
                Smart Blind Control
              </div>
              <h1 className="text-3xl font-semibold tracking-tight sm:text-4xl">
                Gesture Controlled Window Blind
              </h1>
              <p className="mt-2 max-w-xl text-sm text-white/65 sm:text-base">
                A clean control panel for manual movement and automatic light targeting.
              </p>
            </div>

            <div className="grid grid-cols-2 gap-3 sm:w-55">
              <div className="rounded-2xl border border-white/10 bg-black/20 p-3">
                <p className="text-xs text-white/50">Blind Level</p>
                <p className="mt-1 text-2xl font-semibold">{blindLevel}%</p>
              </div>
              <div className="rounded-2xl border border-white/10 bg-black/20 p-3">
                <p className="text-xs text-white/50">Target Light</p>
                <p className="mt-1 text-2xl font-semibold">{targetLight}%</p>
              </div>
            </div>
          </div>

          <div className="grid gap-6 xl:grid-cols-[1fr_0.95fr]">
            <div className="rounded-4xl border border-white/10 bg-linear-to-b from-sky-500/10 via-white/5 to-amber-200/10 p-4 sm:p-6">
              <div className="mb-4 flex items-center justify-between">
                <div>
                  <p className="text-sm text-white/60">Live Animation</p>
                  <p className="text-lg font-medium">Window blind position</p>
                </div>
                <div className="rounded-full border border-white/10 bg-black/20 px-3 py-1 text-xs text-white/70">
                  {blindLevel >= 90 ? "Mostly Open" : blindLevel <= 10 ? "Mostly Closed" : "Partially Open"}
                </div>
              </div>

              <div className="relative mx-auto flex h-107.5 max-w-90 items-center justify-center overflow-hidden rounded-4xl border border-white/10 bg-neutral-900 shadow-inner">
                <motion.div
                  className="absolute inset-0"
                  animate={{ opacity: 0.18 + roomGlow / 140 }}
                  transition={{ duration: 0.4 }}
                >
                  <div className="absolute left-1/2 top-10 h-44 w-44 -translate-x-1/2 rounded-full bg-white blur-3xl" />
                  <div className="absolute inset-x-10 bottom-8 top-28 rounded-3xl bg-linear-to-b from-sky-300/70 via-blue-200/40 to-amber-100/40" />
                </motion.div>

                <div className="absolute left-6 right-6 top-6 bottom-6 rounded-3xl border-4 border-white/20" />
                <div className="absolute left-1/2 top-6 h-8 w-1 -translate-x-1/2 rounded-full bg-white/30" />

                <motion.div
                  className="absolute left-7 right-7 top-7 origin-top rounded-b-[1.2rem] bg-neutral-800/85 backdrop-blur-md"
                  animate={{ height: `${Math.max(closedCoverage * 3.1, 24)}px` }}
                  transition={{ type: "spring", stiffness: 90, damping: 18 }}
                >
                  <div className="h-4 w-full rounded-t-2xl bg-white/10" />
                  <div className="px-2 pt-2">
                    {Array.from({ length: slatCount }).map((_, i) => (
                      <motion.div
                        key={i}
                        className="mb-2 h-5 rounded-md border border-white/5 bg-white/10"
                        animate={{ opacity: 0.35 + (closedCoverage / 100) * 0.55 }}
                        transition={{ duration: 0.3 }}
                      />
                    ))}
                  </div>
                </motion.div>

                <motion.div
                  className="absolute right-8 top-7 h-52.5 w-0.5 bg-white/25"
                  animate={{ height: `${110 + closedCoverage * 1.2}px` }}
                  transition={{ type: "spring", stiffness: 100, damping: 20 }}
                />
                <motion.div
                  className="absolute right-6.25 rounded-full border border-white/15 bg-white/10 shadow-lg backdrop-blur-md"
                  animate={{ top: `${100 + closedCoverage * 1.1}px` }}
                  transition={{ type: "spring", stiffness: 100, damping: 20 }}
                  style={{ width: 18, height: 18 }}
                />
              </div>
            </div>

            <div className="space-y-5">
              <div className="rounded-4xl border border-white/10 bg-white/5 p-5">
                <div className="mb-4 flex items-center gap-2">
                  <SlidersHorizontal className="h-5 w-5 text-white/70" />
                  <h2 className="text-lg font-medium">Manual Controls</h2>
                </div>

                <div className="grid grid-cols-2 gap-3">
                  <button
                    onClick={increment}
                    className="rounded-2xl border border-white/10 bg-white/10 p-4 text-left transition hover:bg-white/15"
                  >
                    <ArrowUp className="mb-3 h-5 w-5" />
                    <p className="font-medium">Increment</p>
                    <p className="text-sm text-white/55">Raise by 10%</p>
                  </button>

                  <button
                    onClick={decrement}
                    className="rounded-2xl border border-white/10 bg-white/10 p-4 text-left transition hover:bg-white/15"
                  >
                    <ArrowDown className="mb-3 h-5 w-5" />
                    <p className="font-medium">Decrement</p>
                    <p className="text-sm text-white/55">Lower by 10%</p>
                  </button>

                  <button
                    onClick={openAll}
                    className="rounded-2xl border border-white/10 bg-linear-to-br from-white/15 to-white/5 p-4 text-left transition hover:scale-[1.01]"
                  >
                    <Maximize2 className="mb-3 h-5 w-5" />
                    <p className="font-medium">Open All</p>
                    <p className="text-sm text-white/55">Set blind to 100%</p>
                  </button>

                  <button
                    onClick={closeAll}
                    className="rounded-2xl border border-white/10 bg-linear-to-br from-white/15 to-white/5 p-4 text-left transition hover:scale-[1.01]"
                  >
                    <Minimize2 className="mb-3 h-5 w-5" />
                    <p className="font-medium">Close All</p>
                    <p className="text-sm text-white/55">Set blind to 0%</p>
                  </button>
                </div>
              </div>

              <div className="rounded-4xl border border-white/10 bg-white/5 p-5">
                <div className="mb-4 flex items-center justify-between gap-3">
                  <div className="flex items-center gap-2">
                    <Sun className="h-5 w-5 text-white/70" />
                    <h2 className="text-lg font-medium">Desired Light Level</h2>
                  </div>
                  <button
                    onClick={() => setAutoMode((prev) => !prev)}
                    className={`rounded-full px-3 py-1 text-xs font-medium transition ${
                      autoMode
                        ? "bg-white text-black"
                        : "border border-white/15 bg-white/5 text-white/70"
                    }`}
                  >
                    {autoMode ? "Auto Adjust On" : "Auto Adjust Off"}
                  </button>
                </div>

                <div className="rounded-2xl border border-white/10 bg-black/20 p-4">
                  <div className="mb-3 flex items-center justify-between text-sm text-white/60">
                    <span>Darker</span>
                    <span>Brighter</span>
                  </div>

                  <input
                    type="range"
                    min="0"
                    max="100"
                    value={targetLight}
                    onChange={(e) => setTargetLight(Number(e.target.value))}
                    className="h-2 w-full cursor-pointer appearance-none rounded-full bg-white/15 accent-white"
                  />

                  <div className="mt-4 flex items-center justify-between">
                    <p className="text-sm text-white/55">
                      Pick the room brightness you want. When auto mode is on, the blind gradually shifts toward it.
                    </p>
                    <div className="ml-4 rounded-2xl border border-white/10 bg-white/10 px-4 py-2 text-xl font-semibold">
                      {targetLight}%
                    </div>
                  </div>
                </div>
              </div>

              <div className="grid gap-4 sm:grid-cols-2">
                <div className="rounded-4xl border border-white/10 bg-white/5 p-5">
                  <p className="text-sm text-white/55">System Status</p>
                  <p className="mt-2 text-xl font-semibold">Online</p>
                  <p className="mt-2 text-sm text-white/60">
                    Ready to connect to your ESP32 or backend API routes.
                  </p>
                </div>
                <div className="rounded-[2rem] border border-white/10 bg-white/5 p-5">
                  <p className="text-sm text-white/55">Control Mode</p>
                  <p className="mt-2 text-xl font-semibold">{autoMode ? "Hybrid" : "Manual"}</p>
                  <p className="mt-2 text-sm text-white/60">
                    Manual buttons with optional light-based adjustment.
                  </p>
                </div>
              </div>
            </div>
          </div>
        </motion.div>

        <motion.div
          initial={{ opacity: 0, x: 20 }}
          animate={{ opacity: 1, x: 0 }}
          transition={{ duration: 0.55, delay: 0.1 }}
          className="rounded-3xl border border-white/10 bg-white/5 p-5 shadow-2xl backdrop-blur-xl sm:p-7"
        >
          <h2 className="text-2xl font-semibold tracking-tight">How to connect this</h2>
          <div className="mt-5 space-y-4 text-sm leading-6 text-white/70">
            <div className="rounded-2xl border border-white/10 bg-black/20 p-4">
              <p className="font-medium text-white">1. Use this as your page component</p>
              <p className="mt-1">
                Put this into <span className="text-white">app/page.tsx</span> or turn it into a reusable component inside your Next.js project.
              </p>
            </div>

            <div className="rounded-2xl border border-white/10 bg-black/20 p-4">
              <p className="font-medium text-white">2. Send commands to your controller</p>
              <p className="mt-1">
                Replace the local state updates with fetch calls to your ESP32 or backend API for increment, decrement, open, close, and target light level.
              </p>
            </div>

            <div className="rounded-2xl border border-white/10 bg-black/20 p-4">
              <p className="font-medium text-white">3. Sync real sensor data</p>
              <p className="mt-1">
                Feed the actual blind position and measured light level back into the UI so the animation reflects the real hardware state.
              </p>
            </div>
          </div>

          <div className="mt-6 rounded-4xl border border-white/10 bg-linear-to-br from-white/10 to-white/5 p-5">
            <p className="text-sm text-white/55">Nice upgrade ideas</p>
            <ul className="mt-3 space-y-3 text-sm text-white/75">
              <li>• Add gesture event logs and sensor readings</li>
              <li>• Add presets like Morning, Study, Privacy, Sleep</li>
              <li>• Add MQTT or WebSocket live updates</li>
              <li>• Add a camera preview or gesture detection status badge</li>
            </ul>
          </div>
        </motion.div>
      </div>
    </div>
  );
}
