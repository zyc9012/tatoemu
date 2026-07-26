import { CheckCircle2, CircleAlert, Info, LoaderCircle } from 'lucide-preact';
import { useEffect, useRef, useState } from 'preact/hooks';
import { CONFIG_DEFAULTS, createDefaultKeyBindings } from './configuration';
import { CheatDialog } from './components/CheatDialog';
import { ConfigDialog } from './components/ConfigDialog';
import { GameLibrary } from './components/GameLibrary';
import { Toolbar } from './components/Toolbar';
import { emulatorRuntime, type StoredFile } from './emulator/runtime';

type NoticeType = 'success' | 'info' | 'error';

const NOTICE_ICONS = {
  success: CheckCircle2,
  info: Info,
  error: CircleAlert,
};

export function App() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [status, setStatus] = useState('Loading emulator...');
  const [notice, setNotice] = useState<{ message: string; type: NoticeType } | null>(null);
  const [ready, setReady] = useState(false);
  const [busyMessage, setBusyMessage] = useState<string | null>(null);
  const [libraryOpen, setLibraryOpen] = useState(false);
  const [cheatsOpen, setCheatsOpen] = useState(false);
  const [configOpen, setConfigOpen] = useState(false);
  const [files, setFiles] = useState<StoredFile[]>([]);
  const [loadedFile, setLoadedFile] = useState('');
  const [config, setConfig] = useState({ ...CONFIG_DEFAULTS });
  const [keyBindings, setKeyBindings] = useState(createDefaultKeyBindings);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const onContextLost = (event: Event) => {
      event.preventDefault();
      setNotice({ message: 'WebGL context lost. Reload the page to continue.', type: 'error' });
    };
    canvas.addEventListener('webglcontextlost', onContextLost);

    void emulatorRuntime.initialize(canvas, {
      onStatus: (message) => setStatus(message),
      onLog: (message, isError) => {
        (isError ? console.error : console.log)(message);
        setNotice({ message, type: isError ? 'error' : 'info' });
      },
    }).then(() => {
      const settings = emulatorRuntime.loadSettings();
      emulatorRuntime.applySettings(settings.config, settings.keyBindings);
      setConfig(settings.config);
      setKeyBindings(settings.keyBindings);
      setFiles(emulatorRuntime.listFiles());
      setReady(true);
      setLibraryOpen(true);
      setStatus('');
    }).catch((reason: unknown) => {
      setNotice({
        message: reason instanceof Error ? reason.message : String(reason),
        type: 'error',
      });
    });

    return () => canvas.removeEventListener('webglcontextlost', onContextLost);
  }, []);

  useEffect(() => {
    if (!notice) return;
    const timeout = window.setTimeout(() => setNotice(null), 5000);
    return () => window.clearTimeout(timeout);
  }, [notice]);

  const loadFile = async (file: StoredFile) => {
    if (isSupportFile(file.name)) {
      setNotice({ message: 'Choose a game ROM to start the emulator.', type: 'info' });
      return;
    }

    setBusyMessage('Loading game...');
    try {
      emulatorRuntime.applySettings(config, keyBindings);
      if (!await emulatorRuntime.loadRom(file.path, updateBiosDownloadState)) {
        throw new Error(`Could not load ${file.name}`);
      }
      setLoadedFile(file.name);
      setLibraryOpen(false);
      setNotice({ message: `${file.name} loaded.`, type: 'success' });
    } catch (error) {
      setNotice({ message: errorMessage(error), type: 'error' });
    } finally {
      setBusyMessage(null);
    }
  };

  const uploadFile = async (file: File) => {
    setBusyMessage('Importing...');
    try {
      const storedFile = await emulatorRuntime.storeFile(file);
      setFiles(emulatorRuntime.listFiles());
      if (isSupportFile(file.name)) {
        setNotice({ message: `${file.name} stored in this browser.`, type: 'success' });
      } else {
        emulatorRuntime.applySettings(config, keyBindings);
        setBusyMessage('Loading game...');
        if (!await emulatorRuntime.loadRom(storedFile.path, updateBiosDownloadState)) {
          throw new Error(`Could not load ${file.name}`);
        }
        setLoadedFile(file.name);
        setLibraryOpen(false);
        setNotice({ message: `${file.name} loaded.`, type: 'success' });
      }
    } catch (error) {
      setNotice({ message: errorMessage(error), type: 'error' });
    } finally {
      setBusyMessage(null);
    }
  };

  const updateBiosDownloadState = (name: string, downloading: boolean) => {
    setBusyMessage(downloading ? `Downloading ${name}...` : 'Loading game...');
  };

  const deleteFile = (file: StoredFile) => {
    try {
      emulatorRuntime.deleteFile(file.path);
      setFiles(emulatorRuntime.listFiles());
      setNotice({ message: `${file.name} deleted.`, type: 'success' });
    } catch (error) {
      setNotice({ message: errorMessage(error), type: 'error' });
    }
  };

  const refreshFiles = () => {
    try {
      setFiles(emulatorRuntime.listFiles());
    } catch (error) {
      setNotice({ message: errorMessage(error), type: 'error' });
    }
  };

  const visibleNotice = busyMessage
    ? { message: busyMessage, type: 'info' as const }
    : notice;
  const NoticeIcon = busyMessage
    ? LoaderCircle
    : visibleNotice ? NOTICE_ICONS[visibleNotice.type] : Info;

  return (
    <main class="app-shell">
      <Toolbar
        gameTitle={loadedFile || 'TatoEmu'}
        ready={ready}
        onOpenLibrary={() => setLibraryOpen(true)}
        onOpenCheats={() => setCheatsOpen(true)}
        onOpenConfig={() => setConfigOpen(true)}
      />
      <section class="canvas-container">
        {status && <div class="status" role="status">{status}</div>}
        <canvas
          id="canvas"
          ref={canvasRef}
          tabIndex={-1}
          onContextMenu={(event) => event.preventDefault()}
        />
        {visibleNotice && (
          <div
            class={`notice ${visibleNotice.type}${busyMessage ? ' busy' : ''}`}
            role={visibleNotice.type === 'error' ? 'alert' : 'status'}
            aria-live={visibleNotice.type === 'error' ? 'assertive' : 'polite'}
            aria-busy={Boolean(busyMessage)}
          >
            <NoticeIcon aria-hidden="true" />
            <span>{visibleNotice.message}</span>
          </div>
        )}
      </section>
      <GameLibrary
        open={libraryOpen}
        files={files}
        loadedFile={loadedFile}
        busy={busyMessage !== null}
        onClose={() => setLibraryOpen(false)}
        onLoad={loadFile}
        onUpload={(file) => void uploadFile(file)}
        onDownload={(file) => emulatorRuntime.readFile(file.path)}
        onDelete={deleteFile}
        onRefresh={refreshFiles}
      />
      <CheatDialog
        open={cheatsOpen}
        romLoaded={Boolean(loadedFile)}
        onClose={() => setCheatsOpen(false)}
        onNotice={(message, type = 'success') => setNotice({ message, type })}
      />
      <ConfigDialog
        open={configOpen}
        config={config}
        keyBindings={keyBindings}
        onClose={() => setConfigOpen(false)}
        onApply={(nextConfig, nextBindings) => {
          try {
            emulatorRuntime.applySettings(nextConfig, nextBindings);
            emulatorRuntime.saveSettings(nextConfig, nextBindings);
            setConfig(nextConfig);
            setKeyBindings(nextBindings);
            setNotice({ message: 'Configuration applied.', type: 'success' });
          } catch (error) {
            setNotice({ message: errorMessage(error), type: 'error' });
          }
        }}
      />
    </main>
  );
}

function isSupportFile(name: string): boolean {
  const extension = name.split('.').pop()?.toLowerCase();
  return extension === 'sav'
    || extension === 'state'
    || name.toLowerCase() === 'neogeo.zip'
    || name.toLowerCase() === 'gba_bios.bin';
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
