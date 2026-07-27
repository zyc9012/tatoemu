import { ChevronDown, ChevronLeft, ChevronRight, ChevronUp } from 'lucide-preact';
import { useEffect, useRef, useState } from 'preact/hooks';
import type { KeyBindings } from '../configuration';
import type { CoreType } from '../emulator/runtime';
import './VirtualControls.css';

interface VirtualControlsProps {
  core: CoreType;
  keyBindings: KeyBindings;
  onInput(keyName: string, pressed: boolean): void;
}

interface ControlDefinition {
  binding: string;
  label: string;
}

interface ControlLayout {
  section: string;
  directions: Record<'up' | 'down' | 'left' | 'right', string>;
  actions: ControlDefinition[];
  system: ControlDefinition[];
  shoulders?: ControlDefinition[];
}

const STANDARD_DIRECTIONS = { up: 'Up', down: 'Down', left: 'Left', right: 'Right' };

const CONTROL_LAYOUTS: Partial<Record<CoreType, ControlLayout>> = {
  gb: {
    section: 'GB',
    directions: STANDARD_DIRECTIONS,
    actions: [{ binding: 'B', label: 'B' }, { binding: 'A', label: 'A' }],
    system: [{ binding: 'Select', label: 'Select' }, { binding: 'Start', label: 'Start' }],
  },
  gba: {
    section: 'GBA',
    directions: STANDARD_DIRECTIONS,
    actions: [{ binding: 'B', label: 'B' }, { binding: 'A', label: 'A' }],
    system: [{ binding: 'Select', label: 'Select' }, { binding: 'Start', label: 'Start' }],
    shoulders: [{ binding: 'L', label: 'L' }, { binding: 'R', label: 'R' }],
  },
  nes: {
    section: 'NES',
    directions: STANDARD_DIRECTIONS,
    actions: [{ binding: 'B', label: 'B' }, { binding: 'A', label: 'A' }],
    system: [{ binding: 'Select', label: 'Select' }, { binding: 'Start', label: 'Start' }],
  },
  cps: {
    section: 'CPS',
    directions: { up: 'P1_Up', down: 'P1_Down', left: 'P1_Left', right: 'P1_Right' },
    actions: [
      { binding: 'P1_Punch1', label: 'LP' },
      { binding: 'P1_Punch2', label: 'MP' },
      { binding: 'P1_Punch3', label: 'HP' },
      { binding: 'P1_Kick1', label: 'LK' },
      { binding: 'P1_Kick2', label: 'MK' },
      { binding: 'P1_Kick3', label: 'HK' },
    ],
    system: [{ binding: 'P1_Coin', label: 'Coin' }, { binding: 'P1_Start', label: 'Start' }],
  },
  neogeo: {
    section: 'NeoGeo',
    directions: { up: 'P1_Up', down: 'P1_Down', left: 'P1_Left', right: 'P1_Right' },
    actions: [
      { binding: 'P1_A', label: 'A' },
      { binding: 'P1_B', label: 'B' },
      { binding: 'P1_C', label: 'C' },
      { binding: 'P1_D', label: 'D' },
    ],
    system: [
      { binding: 'P1_Coin', label: 'Coin' },
      { binding: 'P1_Select', label: 'Select' },
      { binding: 'P1_Start', label: 'Start' },
    ],
  },
};

export function VirtualControls({ core, keyBindings, onInput }: VirtualControlsProps) {
  const layout = CONTROL_LAYOUTS[core];
  const sourcesRef = useRef(new Map<string, Set<string>>());
  const countsRef = useRef(new Map<string, number>());
  const onInputRef = useRef(onInput);
  const [activeKeys, setActiveKeys] = useState<Set<string>>(new Set());
  onInputRef.current = onInput;

  const updateSource = (source: string, nextKeys: Iterable<string>) => {
    const previous = sourcesRef.current.get(source) ?? new Set<string>();
    const next = new Set(nextKeys);

    for (const key of previous) {
      if (next.has(key)) continue;
      const count = (countsRef.current.get(key) ?? 1) - 1;
      if (count === 0) {
        countsRef.current.delete(key);
        onInputRef.current(key, false);
      } else {
        countsRef.current.set(key, count);
      }
    }

    for (const key of next) {
      if (previous.has(key)) continue;
      const count = countsRef.current.get(key) ?? 0;
      countsRef.current.set(key, count + 1);
      if (count === 0) onInputRef.current(key, true);
    }

    if (next.size === 0) sourcesRef.current.delete(source);
    else sourcesRef.current.set(source, next);
    setActiveKeys(new Set(countsRef.current.keys()));
  };

  const releaseAll = (updateState = true) => {
    for (const key of countsRef.current.keys()) onInputRef.current(key, false);
    sourcesRef.current.clear();
    countsRef.current.clear();
    if (updateState) setActiveKeys(new Set());
  };

  useEffect(() => {
    const releaseOnBlur = () => releaseAll();
    const releaseOnHidden = () => {
      if (document.hidden) releaseAll();
    };
    window.addEventListener('blur', releaseOnBlur);
    document.addEventListener('visibilitychange', releaseOnHidden);
    return () => {
      window.removeEventListener('blur', releaseOnBlur);
      document.removeEventListener('visibilitychange', releaseOnHidden);
      releaseAll(false);
    };
  }, [core, keyBindings]);

  if (!layout) return null;

  const bindings = keyBindings[layout.section] ?? {};
  const keyFor = (binding: string) => bindings[binding];
  const directions = Object.fromEntries(
    Object.entries(layout.directions).map(([direction, binding]) => [direction, keyFor(binding)]),
  ) as Record<'up' | 'down' | 'left' | 'right', string>;

  return (
    <div class={`virtual-controls ${core}`} aria-label="Virtual game controls">
      {layout.shoulders && (
        <div class="virtual-shoulders">
          {layout.shoulders.map((control) => (
            <VirtualButton
              key={control.binding}
              control={control}
              inputKey={keyFor(control.binding)}
              active={activeKeys.has(keyFor(control.binding))}
              updateSource={updateSource}
            />
          ))}
        </div>
      )}
      <VirtualDpad directions={directions} activeKeys={activeKeys} updateSource={updateSource} />
      <div class={`virtual-actions count-${layout.actions.length}`} aria-label="Action buttons">
        {layout.actions.map((control) => (
          <VirtualButton
            key={control.binding}
            control={control}
            inputKey={keyFor(control.binding)}
            active={activeKeys.has(keyFor(control.binding))}
            updateSource={updateSource}
          />
        ))}
      </div>
      <div class={`virtual-system count-${layout.system.length}`} aria-label="System buttons">
        {layout.system.map((control) => (
          <VirtualButton
            key={control.binding}
            control={control}
            inputKey={keyFor(control.binding)}
            active={activeKeys.has(keyFor(control.binding))}
            updateSource={updateSource}
          />
        ))}
      </div>
    </div>
  );
}

interface VirtualButtonProps {
  control: ControlDefinition;
  inputKey: string;
  active: boolean;
  updateSource(source: string, keys: Iterable<string>): void;
}

function VirtualButton({ control, inputKey, active, updateSource }: VirtualButtonProps) {
  const release = (pointerId: number) => updateSource(`button:${pointerId}`, []);

  return (
    <button
      class={`virtual-button${active ? ' active' : ''}`}
      type="button"
      aria-label={control.label}
      onContextMenu={(event) => event.preventDefault()}
      onPointerDown={(event) => {
        event.preventDefault();
        event.currentTarget.setPointerCapture(event.pointerId);
        updateSource(`button:${event.pointerId}`, [inputKey]);
      }}
      onPointerUp={(event) => release(event.pointerId)}
      onPointerCancel={(event) => release(event.pointerId)}
      onLostPointerCapture={(event) => release(event.pointerId)}
    >
      {control.label}
    </button>
  );
}

interface VirtualDpadProps {
  directions: Record<'up' | 'down' | 'left' | 'right', string>;
  activeKeys: Set<string>;
  updateSource(source: string, keys: Iterable<string>): void;
}

function VirtualDpad({ directions, activeKeys, updateSource }: VirtualDpadProps) {
  const update = (event: PointerEvent & { currentTarget: HTMLDivElement }) => {
    const bounds = event.currentTarget.getBoundingClientRect();
    const horizontal = ((event.clientX - bounds.left) / bounds.width) * 2 - 1;
    const vertical = ((event.clientY - bounds.top) / bounds.height) * 2 - 1;
    const keys: string[] = [];
    if (horizontal < -0.2) keys.push(directions.left);
    if (horizontal > 0.2) keys.push(directions.right);
    if (vertical < -0.2) keys.push(directions.up);
    if (vertical > 0.2) keys.push(directions.down);
    updateSource(`dpad:${event.pointerId}`, keys);
  };

  const release = (pointerId: number) => updateSource(`dpad:${pointerId}`, []);

  return (
    <div
      class="virtual-dpad"
      role="group"
      aria-label="Directional pad"
      onContextMenu={(event) => event.preventDefault()}
      onPointerDown={(event) => {
        event.preventDefault();
        event.currentTarget.setPointerCapture(event.pointerId);
        update(event);
      }}
      onPointerMove={(event) => {
        if (event.currentTarget.hasPointerCapture(event.pointerId)) update(event);
      }}
      onPointerUp={(event) => release(event.pointerId)}
      onPointerCancel={(event) => release(event.pointerId)}
      onLostPointerCapture={(event) => release(event.pointerId)}
    >
      <span class={`dpad-up${activeKeys.has(directions.up) ? ' active' : ''}`}><ChevronUp /></span>
      <span class={`dpad-right${activeKeys.has(directions.right) ? ' active' : ''}`}><ChevronRight /></span>
      <span class={`dpad-down${activeKeys.has(directions.down) ? ' active' : ''}`}><ChevronDown /></span>
      <span class={`dpad-left${activeKeys.has(directions.left) ? ' active' : ''}`}><ChevronLeft /></span>
      <span class="dpad-center" />
    </div>
  );
}