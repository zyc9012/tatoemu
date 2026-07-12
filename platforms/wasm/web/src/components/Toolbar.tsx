import { Code2, FlaskConical, FolderOpen, Settings } from 'lucide-preact';

interface ToolbarProps {
  gameTitle: string;
  ready: boolean;
  onOpenLibrary(): void;
  onOpenCheats(): void;
  onOpenConfig(): void;
}

export function Toolbar({
  gameTitle,
  ready,
  onOpenLibrary,
  onOpenCheats,
  onOpenConfig,
}: ToolbarProps) {
  return (
    <header class="toolbar">
      <div class="brand-mark">
        <img src="/tatoemu.png" alt="" width="24" height="24" />
      </div>
      <div class="game-title" title={gameTitle}>{gameTitle}</div>
      <nav class="toolbar-actions" aria-label="Emulator tools">
        <button class="button primary" type="button" aria-label="Library" onClick={onOpenLibrary} disabled={!ready}>
          <FolderOpen aria-hidden="true" />
          <span>Library</span>
        </button>
        <button class="button" type="button" aria-label="Cheats" onClick={onOpenCheats} disabled={!ready}>
          <FlaskConical aria-hidden="true" />
          <span>Cheats</span>
        </button>
        <button class="button" type="button" aria-label="Config" onClick={onOpenConfig} disabled={!ready}>
          <Settings aria-hidden="true" />
          <span>Config</span>
        </button>
        <a
          class="icon-button github-link"
          href="https://github.com/zyc9012/tatoemu"
          target="_blank"
          rel="noreferrer"
          aria-label="TatoEmu on GitHub"
        >
          <Code2 aria-hidden="true" />
        </a>
      </nav>
    </header>
  );
}
