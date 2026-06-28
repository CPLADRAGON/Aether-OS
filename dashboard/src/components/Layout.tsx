'use client';

import { NavItem, MobileNavItem } from '@/components/Navigation';

interface LayoutProps {
  realtimeStatus: 'connecting' | 'online' | 'offline';
  activeTab: 'dashboard' | 'sensors' | 'power';
  onTabChange: (tab: 'dashboard' | 'sensors' | 'power') => void;
  children: React.ReactNode;
}

export default function Layout({ realtimeStatus, activeTab, onTabChange, children }: LayoutProps) {
  const statusConfig = {
    online: {
      dot: 'bg-emerald-500 animate-pulse-dot',
      bg: 'bg-emerald-500/10 border-emerald-500/50',
      text: 'text-emerald-500',
      label: 'ONLINE',
    },
    connecting: {
      dot: 'bg-amber-500',
      bg: 'bg-amber-500/10 border-amber-500/50',
      text: 'text-amber-500',
      label: 'CONNECTING',
    },
    offline: {
      dot: 'bg-red-500',
      bg: 'bg-red-500/10 border-red-500/50',
      text: 'text-red-500',
      label: 'OFFLINE',
    },
  };

  const status = statusConfig[realtimeStatus];

  return (
    <div className="min-h-screen bg-[#050505] text-white selection:bg-[#00f3ff]/30">
      {/* Top Header */}
      <header className="sticky top-0 z-50 flex justify-between items-center px-6 py-3 w-full bg-black/40 backdrop-blur-md border-b border-white/10 shadow-xl">
        <div className="flex items-center gap-6">
          <span className="font-headline text-xl font-bold text-[#00f3ff] drop-shadow-[0_0_8px_rgba(0,243,245,0.8)] uppercase tracking-wider">
            AETHER_OS
          </span>
          <div className={`flex items-center gap-2 px-3 py-1 rounded-full border ${status.bg} ${status.text}`}>
            <span className={`w-2 h-2 rounded-full ${status.dot}`} />
            <span className="text-[10px] font-headline tracking-widest uppercase">{status.label}</span>
          </div>
        </div>
        <div className="flex items-center gap-4">
          <button className="material-symbols-outlined text-white/40 hover:text-[#00f3ff] transition-colors">
            notifications
          </button>
          <div className="w-8 h-8 rounded-full overflow-hidden border border-[#00f3ff]/30 bg-slate-800 flex items-center justify-center">
            <span className="material-symbols-outlined text-[#00f3ff]/60 text-sm">person</span>
          </div>
        </div>
      </header>

      <div className="flex">
        {/* Desktop Sidebar */}
        <aside className="hidden lg:flex flex-col h-[calc(100vh-56px)] sticky top-14 left-0 bg-black/60 backdrop-blur-xl border-r border-white/10 shadow-2xl w-64 z-40">
          <div className="p-6 mb-4">
            <div className="flex items-center gap-3 mb-1">
              <div className="w-10 h-10 rounded-lg bg-[#00f3ff]/10 flex items-center justify-center border border-[#00f3ff]/20">
                <span className="material-symbols-outlined text-[#00f3ff]" style={{ fontVariationSettings: "'FILL' 1" }}>
                  terminal
                </span>
              </div>
              <div>
                <h3 className="font-headline text-lg font-black text-[#00f3ff]">AETHER_OS</h3>
                <p className="text-[10px] text-white/40 uppercase tracking-tighter">V2.0.4-STABLE</p>
              </div>
            </div>
          </div>
          <nav className="flex flex-col gap-1 px-2">
            <NavItem
              icon="dashboard"
              label="DASHBOARD"
              active={activeTab === 'dashboard'}
              onClick={() => onTabChange('dashboard')}
            />
            <NavItem
              icon="sensors"
              label="SENSORS"
              active={activeTab === 'sensors'}
              onClick={() => onTabChange('sensors')}
            />
            <NavItem
              icon="bolt"
              label="POWER"
              active={activeTab === 'power'}
              onClick={() => onTabChange('power')}
            />
          </nav>
          <div className="mt-auto p-4 border-t border-white/5">
            <div className="flex items-center gap-2 px-4 py-2">
              <span className="material-symbols-outlined text-sm text-white/40">terminal</span>
              <span className="text-xs text-white/40 uppercase tracking-widest">System Logs</span>
            </div>
          </div>
        </aside>

        {/* Main Content */}
        <main className="flex-1 p-6 overflow-y-auto min-h-[calc(100vh-56px)]">
          <div className="max-w-[1600px] mx-auto space-y-6">
            {children}
          </div>
        </main>
      </div>

      {/* Mobile Bottom Navigation */}
      <nav className="lg:hidden fixed bottom-0 left-0 right-0 h-16 bg-black/80 backdrop-blur-xl border-t border-white/10 flex items-center justify-around z-50">
        <MobileNavItem
          icon="dashboard"
          label="DASH"
          active={activeTab === 'dashboard'}
          onClick={() => onTabChange('dashboard')}
        />
        <MobileNavItem
          icon="sensors"
          label="SENSORS"
          active={activeTab === 'sensors'}
          onClick={() => onTabChange('sensors')}
        />
        <MobileNavItem
          icon="bolt"
          label="POWER"
          active={activeTab === 'power'}
          onClick={() => onTabChange('power')}
        />
        <MobileNavItem
          icon="terminal"
          label="LOGS"
          active={false}
          onClick={() => onTabChange('dashboard')}
        />
      </nav>
    </div>
  );
}
