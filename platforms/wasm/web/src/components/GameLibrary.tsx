import { Download, FileArchive, Gamepad2, HardDriveUpload, RefreshCw, Save, Trash2 } from 'lucide-preact';
import { useRef, useState } from 'preact/hooks';
import type { StoredFile } from '../emulator/runtime';
import { Modal } from './Modal';

interface GameLibraryProps {
  open: boolean;
  files: StoredFile[];
  loadedFile: string;
  busy: boolean;
  onClose(): void;
  onLoad(file: StoredFile): void;
  onUpload(file: File): void;
  onDownload(file: StoredFile): Uint8Array;
  onDelete(file: StoredFile): void;
  onRefresh(): void;
}

export function GameLibrary({
  open,
  files,
  loadedFile,
  busy,
  onClose,
  onLoad,
  onUpload,
  onDownload,
  onDelete,
  onRefresh,
}: GameLibraryProps) {
  const inputRef = useRef<HTMLInputElement>(null);
  const dragDepth = useRef(0);
  const [dragging, setDragging] = useState(false);

  const upload = (file?: File) => {
    if (file) onUpload(file);
    if (inputRef.current) inputRef.current.value = '';
  };

  return (
    <Modal open={open} title="Game Library" className="library-modal" onClose={onClose}>
      <div
        class={`upload-zone${dragging ? ' dragging' : ''}`}
        aria-busy={busy}
        onDragEnter={(event) => {
          event.preventDefault();
          dragDepth.current += 1;
          if (event.dataTransfer?.types.includes('Files')) setDragging(true);
        }}
        onDragLeave={(event) => {
          event.preventDefault();
          dragDepth.current -= 1;
          if (dragDepth.current === 0) setDragging(false);
        }}
        onDragOver={(event) => event.preventDefault()}
        onDrop={(event) => {
          event.preventDefault();
          dragDepth.current = 0;
          setDragging(false);
          upload(event.dataTransfer?.files[0]);
        }}
      >
        <button class="button primary" type="button" onClick={() => inputRef.current?.click()} disabled={busy}>
          <HardDriveUpload aria-hidden="true" />
          <span>Upload file</span>
        </button>
        <input
          ref={inputRef}
          class="visually-hidden"
          type="file"
          accept=".gb,.gbc,.gba,.nes,.zip,.sav,.state,.bin"
          onChange={(event) => upload(event.currentTarget.files?.[0])}
        />
        <p>
          Supports game ROMs, saves, states and BIOS files. ROMs can be found at{' '}
          <a href="https://r-roms.github.io/" target="_blank" rel="noreferrer">r/ROMs</a>.
        </p>
      </div>

      <div class="library-heading">
        <h3>Your files</h3>
        <span>{files.length}</span>
        <button
          class="icon-button library-refresh"
          type="button"
          aria-label="Refresh file list"
          title="Refresh file list"
          disabled={busy}
          onClick={onRefresh}
        >
          <RefreshCw aria-hidden="true" />
        </button>
      </div>
      <div class="file-list">
        {files.length === 0 ? (
          <div class="empty-state">Upload a game ROM to begin.</div>
        ) : files.map((file) => {
          const type = getFileType(file.name);
          const TypeIcon = type.icon;
          return (
            <div class={`file-row${loadedFile === file.name ? ' loaded' : ''}`} key={file.path}>
              <TypeIcon class="file-icon" aria-hidden="true" />
              <button class="file-load" type="button" onClick={() => onLoad(file)} disabled={busy}>
                <strong title={file.name}>{file.name}</strong>
                <span>{type.label} · {formatFileSize(file.size)}</span>
              </button>
              <button
                class="icon-button"
                type="button"
                aria-label={`Download ${file.name}`}
                title={`Download ${file.name}`}
                onClick={() => download(file, onDownload(file))}
              >
                <Download aria-hidden="true" />
              </button>
              <button
                class="icon-button danger"
                type="button"
                aria-label={`Delete ${file.name}`}
                title={loadedFile === file.name ? 'The currently loaded game cannot be deleted' : `Delete ${file.name}`}
                disabled={busy || loadedFile === file.name}
                onClick={() => {
                  if (window.confirm(`Delete ${file.name} from this browser?`)) onDelete(file);
                }}
              >
                <Trash2 aria-hidden="true" />
              </button>
            </div>
          );
        })}
      </div>
    </Modal>
  );
}

function getFileType(name: string) {
  const extension = name.split('.').pop()?.toLowerCase();
  if (extension === 'zip') return { label: 'Archive', icon: FileArchive };
  if (extension === 'sav' || extension === 'state') return { label: 'Save', icon: Save };
  return { label: extension?.toUpperCase() || 'File', icon: Gamepad2 };
}

function formatFileSize(bytes: number): string {
  if (bytes === 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB'];
  const unit = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length - 1);
  return `${(bytes / (1024 ** unit)).toFixed(unit === 0 ? 0 : 1)} ${units[unit]}`;
}

function download(file: StoredFile, data: Uint8Array): void {
  const url = URL.createObjectURL(new Blob([data.slice().buffer], { type: 'application/octet-stream' }));
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = file.name;
  anchor.click();
  URL.revokeObjectURL(url);
}
