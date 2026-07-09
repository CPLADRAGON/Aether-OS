'use client';

interface TopTabProps {
  label: string;
  active?: boolean;
  onClick: () => void;
}

export function TopTab({ label, active = false, onClick }: TopTabProps) {
  return (
    <button
      onClick={onClick}
      className={`px-3 py-1.5 text-sm rounded-md transition-colors ${
        active
          ? 'bg-[#1f1f23] text-[#f4f4f5]'
          : 'text-[#a1a1aa] hover:text-[#f4f4f5] hover:bg-[#1f1f23]/60'
      }`}
    >
      {label}
    </button>
  );
}
