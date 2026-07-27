import { CheckCircle2, ChevronDown, CircleAlert, Info, LoaderCircle } from 'lucide-preact';
import { useEffect, useRef, useState } from 'preact/hooks';
import { CONFIG_DEFAULTS, createDefaultKeyBindings } from './configuration';
import { CheatDialog } from './components/CheatDialog';
import { ConfigDialog } from './components/ConfigDialog';
import { GameLibrary } from './components/GameLibrary';
import { Toolbar } from './components/Toolbar';
import { VirtualControls } from './components/VirtualControls';
import { emulatorRuntime, type CoreType, type StoredFile } from './emulator/runtime';

const VIRTUAL_CONTROLS_QUERY = '(hover: none) and (pointer: coarse)';

type NoticeType = 'success' | 'info' | 'error';

const NOTICE_ICONS = {
  success: CheckCircle2,
  info: Info,
  error: CircleAlert,
};

export function App() {
  const appRef = useRef<HTMLElement>(null);
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
  const [loadedCore, setLoadedCore] = useState<CoreType | null>(null);
  const [virtualControlsAvailable, setVirtualControlsAvailable] = useState(
    () => window.matchMedia(VIRTUAL_CONTROLS_QUERY).matches,
  );
  const [fullscreen, setFullscreen] = useState(false);
  const [toolbarVisible, setToolbarVisible] = useState(true);
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

  useEffect(() => {
    const updateFullscreen = () => setFullscreen(document.fullscreenElement === appRef.current);
    document.addEventListener('fullscreenchange', updateFullscreen);
    return () => document.removeEventListener('fullscreenchange', updateFullscreen);
  }, []);

  useEffect(() => {
    setToolbarVisible(!fullscreen || !loadedFile);
  }, [fullscreen, loadedFile]);

  useEffect(() => {
    const mediaQuery = window.matchMedia(VIRTUAL_CONTROLS_QUERY);
    const updateAvailability = () => setVirtualControlsAvailable(mediaQuery.matches);
    mediaQuery.addEventListener('change', updateAvailability);
    return () => mediaQuery.removeEventListener('change', updateAvailability);
  }, []);

  const loadFile = async (file: StoredFile) => {
    if (isSupportFile(file.name)) {
      setNotice({ message: 'Choose a game ROM to start the emulator.', type: 'info' });
      return;
    }

    setBusyMessage('Loading game...');
    try {
      emulatorRuntime.applySettings(config, keyBindings);
      const core = await emulatorRuntime.loadRom(file.path, updateBiosDownloadState);
      if (!core) {
        throw new Error(`Could not load ${file.name}`);
      }
      setLoadedFile(file.name);
      setLoadedCore(core);
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
        const core = await emulatorRuntime.loadRom(storedFile.path, updateBiosDownloadState);
        if (!core) {
          throw new Error(`Could not load ${file.name}`);
        }
        setLoadedFile(file.name);
        setLoadedCore(core);
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

  const toggleFullscreen = async () => {
    try {
      if (document.fullscreenElement) {
        await document.exitFullscreen();
      } else {
        await appRef.current?.requestFullscreen();
      }
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
  const fullscreenGame = fullscreen && Boolean(loadedFile);

  return (
    <main
      class={`app-shell${fullscreenGame ? ' fullscreen-game' : ''}`}
      ref={appRef}
    >
      {(!fullscreenGame || toolbarVisible) && (
        <Toolbar
          gameTitle={loadedFile || 'TatoEmu'}
          ready={ready}
          fullscreen={fullscreen}
          collapsible={fullscreenGame}
          onOpenLibrary={() => setLibraryOpen(true)}
          onOpenCheats={() => setCheatsOpen(true)}
          onOpenConfig={() => setConfigOpen(true)}
          onToggleFullscreen={() => void toggleFullscreen()}
          onCollapse={() => setToolbarVisible(false)}
        />
      )}
      {fullscreenGame && !toolbarVisible && (
        <button
          class="toolbar-reveal"
          type="button"
          aria-label="Show toolbar"
          title="Show toolbar"
          onClick={() => setToolbarVisible(true)}
        >
          <ChevronDown aria-hidden="true" />
        </button>
      )}
      <section class="canvas-container">
        {status && <div class="status" role="status">{status}</div>}
        <canvas
          id="canvas"
          ref={canvasRef}
          tabIndex={-1}
          onContextMenu={(event) => event.preventDefault()}
        />
        {loadedCore && virtualControlsAvailable && (
          <VirtualControls
            core={loadedCore}
            keyBindings={keyBindings}
            onInput={(keyName, pressed) => emulatorRuntime.setVirtualKey(keyName, pressed)}
          />
        )}
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
