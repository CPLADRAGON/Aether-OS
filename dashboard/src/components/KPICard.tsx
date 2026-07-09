'use client';

import { motion } from 'framer-motion';

interface KPICardProps {
  icon: string;
  label: string;
  value: string;
  unit: string;
  status?: 'normal' | 'warn' | 'critical';
  progress: number;
  subLabel?: string;
}

const statusConfig = {
  normal: { text: 'text-[#f4f4f5]', bar: 'bg-[#818cf8]' },
  warn: { text: 'text-[#fbbf24]', bar: 'bg-[#fbbf24]' },
  critical: { text: 'text-[#f87171]', bar: 'bg-[#f87171]' },
};

export default function KPICard({ icon, label, value, unit, status = 'normal', progress, subLabel }: KPICardProps) {
  const cfg = statusConfig[status];

  return (
    <div className="card p-5">
      <div className="flex justify-between items-start mb-4">
        <div>
          <p className="text-xs text-[#6b7280]">{label}</p>
          <div className="flex items-baseline gap-1 mt-1">
            <motion.span
              key={value}
              initial={{ opacity: 0.4, y: -4 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ type: 'spring', stiffness: 300, damping: 20 }}
              className={`text-3xl font-semibold ${cfg.text} inline-block`}
            >
              {value}
            </motion.span>
            <span className="text-sm text-[#a1a1aa]">{unit}</span>
          </div>
        </div>
        <span className="material-symbols-outlined text-[#6b7280]">{icon}</span>
      </div>
      <div className="h-1 w-full bg-[#1f1f23] rounded-full overflow-hidden">
        <motion.div
          className={`h-full ${cfg.bar} rounded-full`}
          initial={{ width: 0 }}
          animate={{ width: `${Math.min(100, Math.max(0, progress))}%` }}
          transition={{ duration: 0.8, ease: 'easeOut' }}
        />
      </div>
      {subLabel && <p className="text-[11px] text-[#6b7280] mt-2">{subLabel}</p>}
    </div>
  );
}
