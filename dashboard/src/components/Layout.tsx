'use client';

import { TopTab } from '@/components/Navigation';

interface LayoutProps {
  realtimeStatus: 'connecting' | 'online' | 'offline';
  activeTab: 'dashboard' | 'power';
  onTabChange: (tab: 'dashboard' | 'power') => void;
  children: React.ReactNode;
}

export default function Layout({ realtimeStatus, activeTab, onTabChange, children }: LayoutProps) {
  const statusConfig = {
    online: {
      dot: 'bg-[#34d399] animate-pulse-dot',
      text: 'text-[#34d399]',
      label: 'Online',
    },
    connecting: {
      dot: 'bg-[#fbbf24]',
      text: 'text-[#fbbf24]',
      label: 'Connecting',
    },
    offline: {
      dot: 'bg-[#f87171]',
      text: 'text-[#f87171]',
      label: 'Offline',
    },
  };

  const status = statusConfig[realtimeStatus];

  return (
    <div className="min-h-screen bg-[#0d0d0f] text-[#f4f4f5]">
      <header className="sticky top-0 z-50 flex justify-between items-center px-4 sm:px-6 py-3 w-full bg-[#0d0d0f] border-b border-[#1f1f23]">
        <div className="flex items-center gap-3 sm:gap-6">
          <span className="text-base font-semibold text-[#f4f4f5]">Aether</span>
          <nav className="flex items-center gap-1">
            <TopTab label="Dashboard" active={activeTab === 'dashboard'} onClick={() => onTabChange('dashboard')} />
            <TopTab label="Power" active={activeTab === 'power'} onClick={() => onTabChange('power')} />
          </nav>
        </div>
        <div className="flex items-center gap-2 sm:gap-4">
          <div className="flex items-center gap-2">
            <span className={`w-2 h-2 rounded-full ${status.dot}`} />
            <span className={`text-xs ${status.text} hidden sm:inline`}>{status.label}</span>
          </div>
          <div className="w-8 h-8 rounded-full bg-[#16161a] border border-[#1f1f23] items-center justify-center hidden sm:flex">
            <span className="material-symbols-outlined text-[#a1a1aa] text-sm">person</span>
          </div>
        </div>
      </header>

      <main className="flex-1 p-4 sm:p-6">
        <div className="max-w-[1600px] mx-auto space-y-6">
          {children}
        </div>
      </main>
    </div>
  );
}
