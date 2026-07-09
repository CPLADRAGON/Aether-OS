'use client';

interface DeviceLog {
  id: number;
  created_at: string;
  message: string;
  level: string;
}

interface SystemLogsProps {
  logs: DeviceLog[];
  formatSGTime: (dateStr: string) => string;
}

const levelBadge: Record<string, string> = {
  ERROR: 'bg-[#f87171]/15 text-[#f87171]',
  WARN: 'bg-[#fbbf24]/15 text-[#fbbf24]',
  INFO: 'bg-[#818cf8]/15 text-[#818cf8]',
  DEBUG: 'bg-[#3f3f46] text-[#a1a1aa]',
};

export default function SystemLogs({ logs, formatSGTime }: SystemLogsProps) {
  if (logs.length === 0) {
    return (
      <div className="bg-[#0d0d0f] rounded-lg border border-[#1f1f23] p-4 flex items-center justify-center min-h-[100px]">
        <p className="text-[#6b7280] text-xs italic">No system logs available.</p>
      </div>
    );
  }

  return (
    <div className="bg-[#0d0d0f] rounded-lg border border-[#1f1f23] p-3 text-[11px] space-y-1.5 overflow-y-auto custom-scrollbar flex-1 max-h-[300px]">
      {logs.map((log) => (
        <div key={log.id} className="flex gap-2 items-baseline">
          <span className="text-[#52525b] font-mono shrink-0">{formatSGTime(log.created_at)}</span>
          <span className={`px-1.5 py-0.5 rounded text-[9px] font-medium shrink-0 ${levelBadge[log.level] || 'bg-[#1f1f23] text-[#a1a1aa]'}`}>
            {log.level}
          </span>
          <span className="text-[#a1a1aa] font-mono">{log.message}</span>
        </div>
      ))}
    </div>
  );
}
