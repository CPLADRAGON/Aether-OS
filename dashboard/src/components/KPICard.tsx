'use client';

import { motion } from 'framer-motion';

interface KPICardProps {
  icon: string;
  label: string;
  value: string;
  unit: string;
  color: 'cyan' | 'magenta' | 'lime' | 'amber' | 'white';
  progress: number;
  subLabel?: string;
}

const colorConfig = {
  cyan: {
    text: 'text-[#00f3ff]',
    bar: 'bg-[#00f3ff]',
    glow: 'shadow-[0_0_8px_rgba(0,243,255,0.6)]',
    border: 'border-[#00f3ff]/20',
  },
  magenta: {
    text: 'text-[#cf5cff]',
    bar: 'bg-[#cf5cff]',
    glow: 'shadow-[0_0_8px_rgba(207,92,255,0.6)]',
    border: 'border-[#cf5cff]/20',
  },
  lime: {
    text: 'text-[#a4f200]',
    bar: 'bg-[#a4f200]',
    glow: 'shadow-[0_0_8px_rgba(164,242,0,0.6)]',
    border: 'border-[#a4f200]/20',
  },
  amber: {
    text: 'text-amber-400',
    bar: 'bg-amber-400',
    glow: 'shadow-[0_0_8px_rgba(251,191,36,0.6)]',
    border: 'border-amber-400/20',
  },
  white: {
    text: 'text-white',
    bar: 'bg-white',
    glow: 'shadow-[0_0_8px_rgba(255,255,255,0.3)]',
    border: 'border-white/20',
  },
};

export default function KPICard({ icon, label, value, unit, color, progress, subLabel }: KPICardProps) {
  const cfg = colorConfig[color];

  return (
    <div className={`glass-panel p-5 rounded-xl group relative overflow-hidden ${cfg.border}`}>
      <div className="flex justify-between items-start mb-4">
        <div>
          <p className="text-[10px] font-headline text-white/40 uppercase tracking-widest">{label}</p>
          <div className="flex items-baseline gap-1 mt-1">
            <motion.span
              key={value}
              initial={{ scale: 1.1, opacity: 0.5, y: -5 }}
              animate={{ scale: 1, opacity: 1, y: 0 }}
              transition={{ type: 'spring', stiffness: 300, damping: 15 }}
              className={`text-4xl font-headline font-bold ${cfg.text} inline-block`}
            >
              {value}
            </motion.span>
            <span className="text-lg text-white/60 font-body">{unit}</span>
          </div>
        </div>
        <span className={`material-symbols-outlined ${cfg.text}/40 group-hover:${cfg.text} transition-colors`}>
          {icon}
        </span>
      </div>
      <div className="h-1.5 w-full bg-white/5 rounded-full overflow-hidden">
        <motion.div
          className={`h-full ${cfg.bar} rounded-full ${cfg.glow}`}
          initial={{ width: 0 }}
          animate={{ width: `${Math.min(100, Math.max(0, progress))}%` }}
          transition={{ duration: 1, ease: 'easeOut' }}
        />
      </div>
      {subLabel && (
        <p className="text-[10px] text-white/30 font-label mt-2 uppercase tracking-tighter">{subLabel}</p>
      )}
    </div>
  );
}
