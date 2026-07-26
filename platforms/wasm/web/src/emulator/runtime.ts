import type {
  EmscriptenModuleOptions,
  RuntimeCallbacks,
  TatoEmuModule,
  TatoEmuModuleFactory,
} from './types';
import {
  CONFIG_DEFAULTS,
  createDefaultKeyBindings,
  type EmulatorConfig,
  type KeyBindings,
} from '../configuration';

const ROM_DIRECTORY = '/roms';
const CONFIG_FILE_PATH = `${ROM_DIRECTORY}/config.json`;
const MODULE_URL = '/emulator/tatoemu.js';
const BIOS_DIRECTORY = '/bios';
const REQUIRED_BIOS: Partial<Record<CoreType, string>> = {
  neogeo: 'neogeo.zip',
  gba: 'gba_bios.bin',
};

type CoreType = 'gb' | 'gba' | 'nes' | 'cps' | 'neogeo' | 'unknown';
type BiosDownloadHandler = (name: string, downloading: boolean) => void;

export interface StoredFile {
  name: string;
  path: string;
  size: number;
}

export interface CheatCode {
  name: string;
  address: number;
  value: number;
  width: number;
  enabled: boolean;
}

export interface SearchCandidate {
  address: number;
  value: number;
}

export type SearchFilter = 'eq' | 'ne' | 'gt' | 'lt' | 'changed' | 'unchanged';

export class EmulatorRuntime {
  private module: TatoEmuModule | null = null;
  private initialization: Promise<void> | null = null;

  initialize(canvas: HTMLCanvasElement, callbacks: RuntimeCallbacks): Promise<void> {
    if (this.initialization) return this.initialization;

    this.initialization = this.createModule(canvas, callbacks);
    return this.initialization;
  }

  get isReady(): boolean {
    return this.module !== null;
  }

  get fs() {
    if (!this.module) throw new Error('Emulator runtime is not ready');
    return this.module.FS;
  }

  call<T>(
    name: string,
    returnType: string | null,
    argumentTypes: string[] = [],
    arguments_: Array<string | number> = [],
  ): T {
    if (!this.module) throw new Error('Emulator runtime is not ready');
    return this.module.ccall(name, returnType, argumentTypes, arguments_) as T;
  }

  loadSettings(): { config: EmulatorConfig; keyBindings: KeyBindings } {
    const keyBindings = createDefaultKeyBindings();
    if (!this.fs.analyzePath(CONFIG_FILE_PATH).exists) {
      return { config: { ...CONFIG_DEFAULTS }, keyBindings };
    }

    try {
      const decoded = new TextDecoder().decode(this.fs.readFile(CONFIG_FILE_PATH));
      const saved = JSON.parse(decoded) as Partial<EmulatorConfig> & {
        keyBindings?: KeyBindings;
      };
      for (const [section, values] of Object.entries(saved.keyBindings ?? {})) {
        if (keyBindings[section]) Object.assign(keyBindings[section], values);
      }
      const { keyBindings: _ignored, ...savedConfig } = saved;
      return {
        config: { ...CONFIG_DEFAULTS, ...savedConfig },
        keyBindings,
      };
    } catch (error) {
      console.error('Could not load configuration', error);
      return { config: { ...CONFIG_DEFAULTS }, keyBindings };
    }
  }

  saveSettings(config: EmulatorConfig, keyBindings: KeyBindings): void {
    const encoded = new TextEncoder().encode(JSON.stringify({ ...config, keyBindings }, null, 2));
    this.fs.writeFile(CONFIG_FILE_PATH, encoded);
  }

  applySettings(config: EmulatorConfig, keyBindings: KeyBindings): void {
    this.call('setScaleMode', null, ['string'], [config.scaleMode]);
    this.call('setVolume', null, ['number'], [config.volume]);
    this.call('setNeoSys', null, ['string'], [config.neoSys]);
    this.call('setNeoBios', null, ['string'], [config.neoBios]);

    for (const [section, values] of Object.entries(keyBindings)) {
      for (const [key, value] of Object.entries(values)) {
        this.call('setKeyBinding', null, ['string', 'string', 'string'], [section, key, value]);
      }
    }
  }

  listFiles(): StoredFile[] {
    return this.fs.readdir(ROM_DIRECTORY)
      .filter((name) => name !== '.' && name !== '..' && name !== 'config.json')
      .map((name) => {
        const path = `${ROM_DIRECTORY}/${name}`;
        return { name, path, size: this.fs.stat(path).size };
      })
      .sort((left, right) => left.name.localeCompare(right.name));
  }

  async storeFile(file: File): Promise<StoredFile> {
    const path = `${ROM_DIRECTORY}/${file.name}`;
    const data = new Uint8Array(await file.arrayBuffer());
    this.fs.writeFile(path, data);
    return { name: file.name, path, size: data.byteLength };
  }

  readFile(path: string): Uint8Array {
    return this.fs.readFile(path);
  }

  deleteFile(path: string): void {
    this.fs.unlink(path);
  }

  async loadRom(path: string, onBiosDownload?: BiosDownloadHandler): Promise<boolean> {
    const coreType = this.call<CoreType>('getCoreType', 'string', ['string'], [path]);
    const biosName = REQUIRED_BIOS[coreType];
    if (biosName) await this.ensureFile(biosName, onBiosDownload);

    return this.call<number>('loadROMFile', 'number', ['string'], [path]) === 1;
  }

  getCheatCodes(): CheatCode[] {
    return JSON.parse(this.call<string>('cheatGetCodesJson', 'string')) as CheatCode[];
  }

  applyCheat(address: number, value: number, width: number): void {
    this.call('cheatApply', null, ['number', 'number', 'number'], [address, value, width]);
  }

  addCheat(name: string, address: number, value: number, width: number): void {
    this.call(
      'cheatAddCode',
      null,
      ['string', 'number', 'number', 'number'],
      [name, address, value, width],
    );
  }

  toggleCheat(index: number): void {
    this.call('cheatToggleCode', null, ['number'], [index]);
  }

  removeCheat(index: number): void {
    this.call('cheatRemoveCode', null, ['number'], [index]);
  }

  resetSearch(width: number): void {
    this.call('searchReset', null, ['number'], [width]);
  }

  filterSearch(filter: SearchFilter, value: number): void {
    this.call('searchFilter', null, ['string', 'number'], [filter, value]);
  }

  getSearchCandidateCount(): number {
    return this.call('searchCandidateCount', 'number');
  }

  getSearchCandidates(maxResults: number): SearchCandidate[] {
    const json = this.call<string>('searchGetCandidatesJson', 'string', ['number'], [maxResults]);
    return JSON.parse(json) as SearchCandidate[];
  }

  private async ensureFile(name: string, onDownload?: BiosDownloadHandler): Promise<void> {
    const path = `${ROM_DIRECTORY}/${name}`;
    if (this.fs.analyzePath(path).exists) return;

    onDownload?.(name, true);
    try {
      const response = await fetch(`${BIOS_DIRECTORY}/${name}`);
      if (!response.ok) {
        throw new Error(`Could not download ${name} (${response.status})`);
      }
      this.fs.writeFile(path, new Uint8Array(await response.arrayBuffer()));
    } finally {
      onDownload?.(name, false);
    }
  }

  private async createModule(
    canvas: HTMLCanvasElement,
    callbacks: RuntimeCallbacks,
  ): Promise<void> {
    const importedModule = (await import(/* @vite-ignore */ MODULE_URL)) as {
      default: TatoEmuModuleFactory;
    };

    let resolvePersistence: () => void;
    let rejectPersistence: (error: Error) => void;
    const persistenceReady = new Promise<void>((resolve, reject) => {
      resolvePersistence = resolve;
      rejectPersistence = reject;
    });

    const options: EmscriptenModuleOptions = {
      canvas,
      locateFile: (path) => `/emulator/${path}`,
      print: (text) => callbacks.onLog(text, false),
      printErr: (text) => callbacks.onLog(text, true),
      setStatus: (text) => callbacks.onStatus(formatStatus(text)),
      onAbort: (reason) => rejectPersistence(new Error(String(reason))),
      preRun: [() => {
        const fs = options.FS;
        if (!fs) {
          rejectPersistence(new Error('Emscripten filesystem is unavailable'));
          return;
        }

        try {
          if (!fs.analyzePath(ROM_DIRECTORY).exists) fs.mkdir(ROM_DIRECTORY);
          fs.mount(fs.filesystems.IDBFS, { autoPersist: true }, ROM_DIRECTORY);
          fs.syncfs(true, (error) => {
            if (error) rejectPersistence(error);
            else resolvePersistence();
          });
        } catch (error) {
          rejectPersistence(asError(error));
        }
      }],
    };

    this.module = await importedModule.default(options);
    await persistenceReady;
  }
}

function formatStatus(text: string): string {
  const progress = text.match(/([^(]+)\((\d+(?:\.\d+)?)\/(\d+)\)/);
  if (!progress) return text;

  const percent = Math.round((Number(progress[2]) / Number(progress[3])) * 100);
  return `${progress[1].trim()} (${percent}%)`;
}

function asError(error: unknown): Error {
  return error instanceof Error ? error : new Error(String(error));
}

export const emulatorRuntime = new EmulatorRuntime();
