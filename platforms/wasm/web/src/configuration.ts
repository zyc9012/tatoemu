export interface EmulatorConfig {
  scaleMode: 'nearest' | 'linear';
  volume: number;
  neoSys: 'mvs' | 'aes';
  neoBios: string;
}

export interface KeyBindingDefinition {
  key: string;
  label: string;
  defaultValue: string;
}

export interface KeyBindingSection {
  label: string;
  groups: Record<string, KeyBindingDefinition[]>;
}

export type KeyBindings = Record<string, Record<string, string>>;

export const CONFIG_DEFAULTS: EmulatorConfig = {
  scaleMode: 'linear',
  volume: 100,
  neoSys: 'mvs',
  neoBios: '19',
};

const binding = (key: string, label: string, defaultValue: string): KeyBindingDefinition => ({
  key,
  label,
  defaultValue,
});

const standardControls = [
  binding('Up', 'D-Pad Up', 'Up'),
  binding('Down', 'D-Pad Down', 'Down'),
  binding('Left', 'D-Pad Left', 'Left'),
  binding('Right', 'D-Pad Right', 'Right'),
  binding('A', 'A Button', 'Z'),
  binding('B', 'B Button', 'X'),
  binding('Start', 'Start', 'Return'),
  binding('Select', 'Select', 'Right Shift'),
];

export const KEY_BINDING_SCHEMAS: Record<string, KeyBindingSection> = {
  Common: {
    label: 'Common',
    groups: {
      System: [
        binding('Quit', 'Quit', 'Escape'),
        binding('Pause', 'Pause/Resume', 'P'),
        binding('SaveState', 'Save State', 'F5'),
        binding('LoadState', 'Load State', 'F9'),
        binding('LoadState_Backup1', 'Load State (Backup 1)', 'F10'),
        binding('LoadState_Backup2', 'Load State (Backup 2)', 'F11'),
        binding('LoadState_Backup3', 'Load State (Backup 3)', 'F12'),
        binding('SpeedUp', 'Speed Up', '='),
        binding('SpeedDown', 'Speed Down', '-'),
      ],
    },
  },
  GB: { label: 'Game Boy', groups: { Controls: standardControls } },
  GBA: {
    label: 'GBA',
    groups: {
      Controls: [
        ...standardControls.slice(0, 6),
        binding('L', 'L Button', 'A'),
        binding('R', 'R Button', 'S'),
        ...standardControls.slice(6),
      ],
    },
  },
  NES: { label: 'NES', groups: { Controls: standardControls } },
  CPS: {
    label: 'CPS1/CPS2',
    groups: {
      'Player 1': [
        binding('P1_Up', 'Up', 'Up'),
        binding('P1_Down', 'Down', 'Down'),
        binding('P1_Left', 'Left', 'Left'),
        binding('P1_Right', 'Right', 'Right'),
        binding('P1_Punch1', 'Punch 1', 'A'),
        binding('P1_Punch2', 'Punch 2', 'S'),
        binding('P1_Punch3', 'Punch 3', 'D'),
        binding('P1_Kick1', 'Kick 1', 'Z'),
        binding('P1_Kick2', 'Kick 2', 'X'),
        binding('P1_Kick3', 'Kick 3', 'C'),
        binding('P1_Start', 'Start', '1'),
        binding('P1_Coin', 'Coin', '5'),
      ],
      'Player 2': [
        binding('P2_Up', 'Up', 'Keypad 8'),
        binding('P2_Down', 'Down', 'Keypad 5'),
        binding('P2_Left', 'Left', 'Keypad 4'),
        binding('P2_Right', 'Right', 'Keypad 6'),
        binding('P2_Punch1', 'Punch 1', 'J'),
        binding('P2_Punch2', 'Punch 2', 'K'),
        binding('P2_Punch3', 'Punch 3', 'L'),
        binding('P2_Kick1', 'Kick 1', 'M'),
        binding('P2_Kick2', 'Kick 2', ','),
        binding('P2_Kick3', 'Kick 3', '.'),
        binding('P2_Start', 'Start', '2'),
        binding('P2_Coin', 'Coin', '6'),
      ],
      System: [
        binding('Diag', 'Diagnostic', 'F2'),
        binding('Service', 'Service', 'F3'),
      ],
    },
  },
  NeoGeo: {
    label: 'NeoGeo',
    groups: {
      'Player 1': [
        binding('P1_Up', 'Up', 'Up'),
        binding('P1_Down', 'Down', 'Down'),
        binding('P1_Left', 'Left', 'Left'),
        binding('P1_Right', 'Right', 'Right'),
        binding('P1_A', 'A', 'A'),
        binding('P1_B', 'B', 'S'),
        binding('P1_C', 'C', 'D'),
        binding('P1_D', 'D', 'F'),
        binding('P1_Start', 'Start', '1'),
        binding('P1_Select', 'Select', '3'),
        binding('P1_Coin', 'Coin', '5'),
      ],
      'Player 2': [
        binding('P2_Up', 'Up', 'Keypad 8'),
        binding('P2_Down', 'Down', 'Keypad 5'),
        binding('P2_Left', 'Left', 'Keypad 4'),
        binding('P2_Right', 'Right', 'Keypad 6'),
        binding('P2_A', 'A', 'J'),
        binding('P2_B', 'B', 'K'),
        binding('P2_C', 'C', 'L'),
        binding('P2_D', 'D', ';'),
        binding('P2_Start', 'Start', '2'),
        binding('P2_Select', 'Select', '4'),
        binding('P2_Coin', 'Coin', '6'),
      ],
      System: [
        binding('Test', 'Test', 'F2'),
        binding('Service', 'Service', 'F3'),
      ],
    },
  },
  MD: {
    label: 'Mega Drive',
    groups: {
      Controls: [
        binding('Up', 'D-Pad Up', 'Up'),
        binding('Down', 'D-Pad Down', 'Down'),
        binding('Left', 'D-Pad Left', 'Left'),
        binding('Right', 'D-Pad Right', 'Right'),
        binding('A', 'A Button', 'Z'),
        binding('B', 'B Button', 'X'),
        binding('C', 'C Button', 'C'),
        binding('X', 'X Button', 'A'),
        binding('Y', 'Y Button', 'S'),
        binding('Z', 'Z Button', 'D'),
        binding('Start', 'Start', 'Return'),
        binding('Mode', 'Mode', 'Right Shift'),
      ],
    },
  },
};

export const BROWSER_KEY_TO_SDL: Record<string, string> = {
  Escape: 'Escape',
  F1: 'F1', F2: 'F2', F3: 'F3', F4: 'F4', F5: 'F5', F6: 'F6',
  F7: 'F7', F8: 'F8', F9: 'F9', F10: 'F10', F11: 'F11', F12: 'F12',
  Backquote: '`', Digit1: '1', Digit2: '2', Digit3: '3', Digit4: '4',
  Digit5: '5', Digit6: '6', Digit7: '7', Digit8: '8', Digit9: '9', Digit0: '0',
  Minus: '-', Equal: '=', Backspace: 'Backspace', Tab: 'Tab',
  KeyA: 'A', KeyB: 'B', KeyC: 'C', KeyD: 'D', KeyE: 'E', KeyF: 'F',
  KeyG: 'G', KeyH: 'H', KeyI: 'I', KeyJ: 'J', KeyK: 'K', KeyL: 'L',
  KeyM: 'M', KeyN: 'N', KeyO: 'O', KeyP: 'P', KeyQ: 'Q', KeyR: 'R',
  KeyS: 'S', KeyT: 'T', KeyU: 'U', KeyV: 'V', KeyW: 'W', KeyX: 'X',
  KeyY: 'Y', KeyZ: 'Z', BracketLeft: '[', BracketRight: ']', Backslash: '\\',
  Semicolon: ';', Quote: "'", Enter: 'Return', Comma: ',', Period: '.', Slash: '/',
  Space: 'Space', CapsLock: 'CapsLock', ShiftLeft: 'Left Shift', ShiftRight: 'Right Shift',
  ControlLeft: 'Left Ctrl', ControlRight: 'Right Ctrl', AltLeft: 'Left Alt',
  AltRight: 'Right Alt', MetaLeft: 'Left GUI', MetaRight: 'Right GUI',
  ArrowUp: 'Up', ArrowDown: 'Down', ArrowLeft: 'Left', ArrowRight: 'Right',
  Insert: 'Insert', Delete: 'Delete', Home: 'Home', End: 'End',
  PageUp: 'PageUp', PageDown: 'PageDown', Numpad0: 'Keypad 0', Numpad1: 'Keypad 1',
  Numpad2: 'Keypad 2', Numpad3: 'Keypad 3', Numpad4: 'Keypad 4',
  Numpad5: 'Keypad 5', Numpad6: 'Keypad 6', Numpad7: 'Keypad 7',
  Numpad8: 'Keypad 8', Numpad9: 'Keypad 9', NumpadDecimal: 'Keypad .',
  NumpadAdd: 'Keypad +', NumpadSubtract: 'Keypad -', NumpadMultiply: 'Keypad *',
  NumpadDivide: 'Keypad /', NumpadEnter: 'Keypad Enter',
};

export const NEO_BIOS_OPTIONS = [
  'MVS Asia/Europe ver. 6 (1 slot)', 'MVS Asia/Europe ver. 5 (1 slot)',
  'MVS Asia/Europe ver. 3 (4 slot)', 'MVS USA ver. 5 (2 slot)',
  'MVS USA ver. 5 (4 slot)', 'MVS USA ver. 5 (6 slot)', 'MVS USA (U4)',
  'MVS USA (U3)', 'MVS Japan ver. 6 (? slot)', 'MVS Japan ver. 5 (? slot)',
  'MVS Japan ver. 3 (4 slot)', 'NEO-MVH MV1C (Asia)', 'NEO-MVH MV1C (Japan)',
  'MVS Japan (J3)', 'MVS Japan (J3, alt)', 'AES Japan', 'AES Asia',
  'Development Kit', 'Deck ver. 6 (Git Ver 1.3)', 'Universe BIOS (Hack, Ver. 4.0)',
  'Universe BIOS (Hack, Ver. 3.3)', 'Universe BIOS (Hack, Ver. 3.2)',
  'Universe BIOS (Hack, Ver. 3.1)', 'Universe BIOS (Hack, Ver. 3.0)',
  'Universe BIOS (Hack, Ver. 2.3)', 'Universe BIOS (Hack, Ver. 2.3)',
  'Universe BIOS (Hack, Ver. 2.2)', 'Universe BIOS (Hack, Ver. 2.1)',
  'Universe BIOS (Hack, Ver. 2.0)', 'Universe BIOS (Hack, Ver. 1.3)',
  'Universe BIOS (Hack, Ver. 1.2)', 'Universe BIOS (Hack, Ver. 1.2)',
  'Universe BIOS (Hack, Ver. 1.1)', 'Universe BIOS (Hack, Ver. 1.0)',
  'NeoOpen BIOS v0.1 beta',
];

export function createDefaultKeyBindings(): KeyBindings {
  return Object.fromEntries(
    Object.entries(KEY_BINDING_SCHEMAS).map(([section, schema]) => [
      section,
      Object.fromEntries(
        Object.values(schema.groups).flat().map((item) => [item.key, item.defaultValue]),
      ),
    ]),
  );
}

export function cloneKeyBindings(bindings: KeyBindings): KeyBindings {
  return Object.fromEntries(
    Object.entries(bindings).map(([section, values]) => [section, { ...values }]),
  );
}
