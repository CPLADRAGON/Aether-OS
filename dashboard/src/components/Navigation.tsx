'use client';

interface NavItemProps {
  icon: string;
  label: string;
  active?: boolean;
  onClick: () => void;
}

export function NavItem({ icon, label, active = false, onClick }: NavItemProps) {
  return (
    <div
      onClick={onClick}
      className={`flex items-center gap-4 p-3 rounded-lg transition-all cursor-pointer ${
        active
          ? 'bg-[#00f3ff]/10 text-[#00f3ff] border-r-4 border-[#00f3ff]'
          : 'text-white/40 hover:text-white/80 hover:bg-white/5'
      }`}
    >
      <span className="material-symbols-outlined">{icon}</span>
      <span className="hidden group-hover:block text-[12px] font-medium tracking-widest">{label}</span>
    </div>
  );
}

export function MobileNavItem({ icon, label, active = false, onClick }: NavItemProps) {
  return (
    <div
      onClick={onClick}
      className={`flex flex-col items-center justify-center gap-1 transition-all cursor-pointer ${
        active ? 'text-[#00f3ff]' : 'text-white/40 hover:text-white/80'
      }`}
    >
      <span className="material-symbols-outlined text-[24px]">{icon}</span>
      <span className="text-[9px] font-bold tracking-widest">{label}</span>
    </div>
  );
}

interface TimeToggleProps {
  label: string;
  active: boolean;
  onClick: () => void;
}

export function TimeToggle({ label, active, onClick }: TimeToggleProps) {
  return (
    <button
      onClick={onClick}
      className={`px-4 py-1.5 text-[10px] rounded font-bold transition-all duration-300 ${
        active
          ? 'bg-[#00f3ff] text-slate-950 shadow-[0_0_15px_rgba(0,243,255,0.4)]'
          : 'text-white/40 hover:text-white/80 hover:bg-white/5'
      }`}
    >
      {label}
    </button>
  );
}
