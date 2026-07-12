export interface EmscriptenFileSystem {
  filesystems: {
    IDBFS: unknown;
  };
  analyzePath(path: string): { exists: boolean };
  mkdir(path: string): void;
  mount(type: unknown, options: Record<string, unknown>, mountpoint: string): void;
  readFile(path: string): Uint8Array;
  readdir(path: string): string[];
  stat(path: string): { size: number };
  syncfs(populate: boolean, callback: (error: Error | null) => void): void;
  unlink(path: string): void;
  writeFile(path: string, data: Uint8Array): void;
}

export interface TatoEmuModule {
  FS: EmscriptenFileSystem;
  ccall(
    name: string,
    returnType: string | null,
    argumentTypes: string[],
    arguments_: Array<string | number>,
  ): unknown;
}

export interface EmscriptenModuleOptions extends Partial<TatoEmuModule> {
  canvas: HTMLCanvasElement;
  locateFile(path: string): string;
  print(text: string): void;
  printErr(text: string): void;
  setStatus(text: string): void;
  preRun: Array<() => void>;
  onAbort(reason: unknown): void;
}

export type TatoEmuModuleFactory = (
  options: EmscriptenModuleOptions,
) => Promise<TatoEmuModule>;

export interface RuntimeCallbacks {
  onLog(message: string, isError: boolean): void;
  onStatus(message: string): void;
}
