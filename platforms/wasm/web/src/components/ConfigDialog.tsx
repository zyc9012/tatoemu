import { Check, RotateCcw } from 'lucide-preact';
import { useEffect, useState } from 'preact/hooks';
import {
  BROWSER_KEY_TO_SDL,
  CONFIG_DEFAULTS,
  KEY_BINDING_SCHEMAS,
  NEO_BIOS_OPTIONS,
  cloneKeyBindings,
  createDefaultKeyBindings,
  type EmulatorConfig,
  type KeyBindingDefinition,
  type KeyBindings,
} from '../configuration';
import { Modal } from './Modal';

interface ConfigDialogProps {
  open: boolean;
  config: EmulatorConfig;
  keyBindings: KeyBindings;
  onClose(): void;
  onApply(config: EmulatorConfig, keyBindings: KeyBindings): void;
}

interface ListeningBinding {
  section: string;
  key: string;
}

export function ConfigDialog({ open, config, keyBindings, onClose, onApply }: ConfigDialogProps) {
  const [activeSection, setActiveSection] = useState('General');
  const [draftConfig, setDraftConfig] = useState<EmulatorConfig>({ ...config });
  const [draftBindings, setDraftBindings] = useState<KeyBindings>(() => cloneKeyBindings(keyBindings));
  const [listening, setListening] = useState<ListeningBinding | null>(null);

  useEffect(() => {
    if (!open) return;
    setDraftConfig({ ...config });
    setDraftBindings(cloneKeyBindings(keyBindings));
    setListening(null);
  }, [open, config, keyBindings]);

  useEffect(() => {
    if (!listening) return;

    const capture = (event: KeyboardEvent) => {
      event.preventDefault();
      event.stopPropagation();
      const sdlName = BROWSER_KEY_TO_SDL[event.code];
      if (sdlName) {
        setDraftBindings((current) => ({
          ...current,
          [listening.section]: {
            ...current[listening.section],
            [listening.key]: sdlName,
          },
        }));
      }
      setListening(null);
    };

    document.addEventListener('keydown', capture, true);
    return () => document.removeEventListener('keydown', capture, true);
  }, [listening]);

  const reset = () => {
    const defaults = createDefaultKeyBindings();
    const defaultConfig = { ...CONFIG_DEFAULTS };
    setDraftConfig(defaultConfig);
    setDraftBindings(defaults);
    setListening(null);
    onApply(defaultConfig, defaults);
  };

  return (
    <Modal open={open} title="Configuration" className="config-modal" onClose={onClose}>
      <div class="config-layout">
        <nav class="config-tabs" aria-label="Configuration sections">
          <button class={activeSection === 'General' ? 'active' : ''} type="button" onClick={() => setActiveSection('General')}>
            General
          </button>
          {Object.entries(KEY_BINDING_SCHEMAS).filter(([section]) => section !== 'Common').map(([section, schema]) => (
            <button class={activeSection === section ? 'active' : ''} type="button" onClick={() => setActiveSection(section)}>
              {schema.label}
            </button>
          ))}
        </nav>

        <div class="config-panel">
          {activeSection === 'General' ? (
            <>
              <section class="settings-block">
                <h3>Display and audio</h3>
                <label class="setting-row">
                  <span>Scale mode</span>
                  <select
                    value={draftConfig.scaleMode}
                    onChange={(event) => setDraftConfig({
                      ...draftConfig,
                      scaleMode: event.currentTarget.value as EmulatorConfig['scaleMode'],
                    })}
                  >
                    <option value="nearest">Nearest</option>
                    <option value="linear">Linear</option>
                  </select>
                </label>
                <label class="setting-row">
                  <span>Volume</span>
                  <input
                    type="range"
                    min="0"
                    max="100"
                    value={draftConfig.volume}
                    onInput={(event) => setDraftConfig({
                      ...draftConfig,
                      volume: Number(event.currentTarget.value),
                    })}
                  />
                  <output>{draftConfig.volume}%</output>
                </label>
              </section>
              <BindingGroup
                section="Common"
                title="System shortcuts"
                bindings={KEY_BINDING_SCHEMAS.Common.groups.System}
                values={draftBindings.Common}
                listening={listening}
                onListen={(key) => setListening({ section: 'Common', key })}
              />
            </>
          ) : (
            <SystemSettings
              section={activeSection}
              config={draftConfig}
              keyBindings={draftBindings}
              listening={listening}
              onConfigChange={setDraftConfig}
              onListen={(section, key) => setListening({ section, key })}
            />
          )}
        </div>
      </div>

      <footer class="dialog-actions">
        <button class="button" type="button" onClick={reset}>
          <RotateCcw aria-hidden="true" /> Reset defaults
        </button>
        <button class="button primary" type="button" onClick={() => {
          onApply(draftConfig, draftBindings);
          onClose();
        }}>
          <Check aria-hidden="true" /> Apply
        </button>
      </footer>
    </Modal>
  );
}

interface SystemSettingsProps {
  section: string;
  config: EmulatorConfig;
  keyBindings: KeyBindings;
  listening: ListeningBinding | null;
  onConfigChange(config: EmulatorConfig): void;
  onListen(section: string, key: string): void;
}

function SystemSettings({
  section,
  config,
  keyBindings,
  listening,
  onConfigChange,
  onListen,
}: SystemSettingsProps) {
  const schema = KEY_BINDING_SCHEMAS[section];
  if (!schema) return null;

  const groups = Object.entries(schema.groups);
  const playerGroups = groups.filter(([name]) => name.startsWith('Player'));
  const remainingGroups = groups.filter(([name]) => !name.startsWith('Player'));

  return (
    <>
      {section === 'NeoGeo' && (
        <section class="settings-block">
          <h3>NeoGeo system</h3>
          <label class="setting-row">
            <span>Type</span>
            <select value={config.neoSys} onChange={(event) => onConfigChange({
              ...config,
              neoSys: event.currentTarget.value as EmulatorConfig['neoSys'],
            })}>
              <option value="mvs">MVS</option>
              <option value="aes">AES</option>
            </select>
          </label>
          <label class="setting-row">
            <span>BIOS</span>
            <select value={config.neoBios} onChange={(event) => onConfigChange({
              ...config,
              neoBios: event.currentTarget.value,
            })}>
              {NEO_BIOS_OPTIONS.map((label, index) => (
                <option value={String(index)}>{label}</option>
              ))}
            </select>
          </label>
        </section>
      )}

      {playerGroups.length >= 2 ? (
        <div class="player-grid">
          {playerGroups.map(([title, bindings]) => (
            <BindingGroup
              section={section}
              title={title}
              bindings={bindings}
              values={keyBindings[section]}
              listening={listening}
              onListen={(key) => onListen(section, key)}
            />
          ))}
        </div>
      ) : groups.map(([title, bindings]) => (
        <BindingGroup
          section={section}
          title={title}
          bindings={bindings}
          values={keyBindings[section]}
          listening={listening}
          onListen={(key) => onListen(section, key)}
        />
      ))}

      {playerGroups.length >= 2 && remainingGroups.map(([title, bindings]) => (
        <BindingGroup
          section={section}
          title={title}
          bindings={bindings}
          values={keyBindings[section]}
          listening={listening}
          onListen={(key) => onListen(section, key)}
        />
      ))}
    </>
  );
}

interface BindingGroupProps {
  section: string;
  title: string;
  bindings: KeyBindingDefinition[];
  values: Record<string, string>;
  listening: ListeningBinding | null;
  onListen(key: string): void;
}

function BindingGroup({ section, title, bindings, values, listening, onListen }: BindingGroupProps) {
  return (
    <section class="binding-group">
      <h3>{title}</h3>
      {bindings.map((item) => {
        const isListening = listening?.section === section && listening.key === item.key;
        return (
          <div class="binding-row" key={item.key}>
            <span>{item.label}</span>
            <button
              class={`key-button${isListening ? ' listening' : ''}`}
              type="button"
              onClick={() => onListen(item.key)}
            >
              {isListening ? 'Press a key...' : values[item.key] ?? item.defaultValue}
            </button>
          </div>
        );
      })}
    </section>
  );
}
