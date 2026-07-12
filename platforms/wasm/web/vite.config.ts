import { readFile, mkdir, copyFile } from 'node:fs/promises';
import { dirname, extname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import preact from '@preact/preset-vite';
import { defineConfig, type Plugin } from 'vite';

const webDirectory = dirname(fileURLToPath(import.meta.url));
const repositoryRoot = resolve(webDirectory, '../../..');
const cmakeBuildDirectory = process.env.TATOEMU_BUILD_DIR
  ? resolve(process.env.TATOEMU_BUILD_DIR)
  : resolve(repositoryRoot, 'build-wasm');
const emulatorDirectory = resolve(cmakeBuildDirectory, 'emulator');
const distDirectory = resolve(cmakeBuildDirectory, 'dist');

function emulatorArtifacts(): Plugin {
  return {
    name: 'tatoemu-emulator-artifacts',
    configureServer(server) {
      server.middlewares.use(async (request, response, next) => {
        const match = request.url?.match(/^\/emulator\/(tatoemu\.(?:js|wasm))$/);
        if (!match) {
          next();
          return;
        }

        try {
          const artifact = await readFile(resolve(emulatorDirectory, match[1]));
          response.statusCode = 200;
          response.setHeader(
            'Content-Type',
            extname(match[1]) === '.wasm' ? 'application/wasm' : 'text/javascript',
          );
          response.end(artifact);
        } catch (error) {
          next(error as Error);
        }
      });
    },
    async closeBundle() {
      const outputDirectory = resolve(distDirectory, 'emulator');
      await mkdir(outputDirectory, { recursive: true });
      await Promise.all([
        copyFile(resolve(emulatorDirectory, 'tatoemu.js'), resolve(outputDirectory, 'tatoemu.js')),
        copyFile(resolve(emulatorDirectory, 'tatoemu.wasm'), resolve(outputDirectory, 'tatoemu.wasm')),
        copyFile(resolve(repositoryRoot, 'tatoemu.png'), resolve(distDirectory, 'tatoemu.png')),
      ]);
    },
  };
}

export default defineConfig({
  plugins: [preact(), emulatorArtifacts()],
  build: {
    outDir: distDirectory,
    emptyOutDir: true,
  },
});
