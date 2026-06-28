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

const levelColors: Record<string, string> = {
  ERROR: 'text-red-500',
  WARN: 'text-amber-500',
  INFO: 'text-[#00f3ff]',
  DEBUG: 'text-[#a4f200]',
};

export default function SystemLogs({ logs, formatSGTime }: SystemLogsProps) {
  if (logs.length === 0) {
    return (
      <div className="bg-black/40 rounded-lg border border-white/5 p-4 flex items-center justify-center min-h-[100px]">
        <p className="text-white/30 text-xs italic font-mono">No system logs available.</p>
      </div>
    );
  }

  return (
    <div className="bg-black/40 rounded-lg border border-white/5 p-3 font-mono text-[10px] space-y-1 overflow-y-auto custom-scrollbar flex-1 max-h-[300px]">
      {logs.map((log) => (
        <div key={log.id} className="flex gap-2">
          <span className="text-white/20 shrink-0">[{formatSGTime(log.created_at)}]</span>
          <span className={`font-bold shrink-0 ${levelColors[log.level] || 'text-white/60'}`}>
            [{log.level}]
          </span>
          <span className="text-white/60">{log.message}</span>
        </div>
      ))}
    </div>
  );
}
