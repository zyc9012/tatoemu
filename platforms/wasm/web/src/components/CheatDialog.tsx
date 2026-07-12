import { Plus, RefreshCw, Snowflake, Trash2 } from 'lucide-preact';
import { useState } from 'preact/hooks';
import {
  emulatorRuntime,
  type CheatCode,
  type SearchCandidate,
  type SearchFilter,
} from '../emulator/runtime';
import { Modal } from './Modal';

interface CheatDialogProps {
  open: boolean;
  romLoaded: boolean;
  onClose(): void;
  onNotice(message: string, type?: 'success' | 'info' | 'error'): void;
}

const MAX_CANDIDATES = 100;

export function CheatDialog({ open, romLoaded, onClose, onNotice }: CheatDialogProps) {
  const [tab, setTab] = useState<'search' | 'codes'>('search');
  const [width, setWidth] = useState(2);
  const [filter, setFilter] = useState<SearchFilter>('eq');
  const [filterValue, setFilterValue] = useState('');
  const [searchInitialized, setSearchInitialized] = useState(false);
  const [candidateCount, setCandidateCount] = useState(0);
  const [candidates, setCandidates] = useState<SearchCandidate[]>([]);
  const [codes, setCodes] = useState<CheatCode[]>([]);
  const [name, setName] = useState('');
  const [address, setAddress] = useState('');
  const [value, setValue] = useState('');
  const [codeWidth, setCodeWidth] = useState(2);

  const requireRom = () => {
    if (romLoaded) return true;
    onNotice('Load a game first.', 'error');
    return false;
  };

  const refreshCandidates = () => {
    if (!romLoaded || !searchInitialized) return;
    try {
      setCandidateCount(emulatorRuntime.getSearchCandidateCount());
      setCandidates(emulatorRuntime.getSearchCandidates(MAX_CANDIDATES));
    } catch (error) {
      onNotice(errorMessage(error), 'error');
    }
  };

  const refreshCodes = () => {
    if (!romLoaded) {
      setCodes([]);
      return;
    }
    try {
      setCodes(emulatorRuntime.getCheatCodes());
    } catch (error) {
      onNotice(errorMessage(error), 'error');
    }
  };

  const switchTab = (nextTab: 'search' | 'codes') => {
    setTab(nextTab);
    if (nextTab === 'codes') refreshCodes();
  };

  const resetSearch = () => {
    if (!requireRom()) return;
    emulatorRuntime.resetSearch(width);
    setSearchInitialized(true);
    const count = emulatorRuntime.getSearchCandidateCount();
    setCandidateCount(count);
    setCandidates(emulatorRuntime.getSearchCandidates(MAX_CANDIDATES));
  };

  const applyFilter = () => {
    if (!requireRom()) return;
    if (!searchInitialized) {
      onNotice('Reset the search first.', 'error');
      return;
    }

    const parsedValue = filter === 'eq' || filter === 'ne' ? parseNumber(filterValue) : 0;
    if (Number.isNaN(parsedValue)) {
      onNotice('Enter a valid filter value.', 'error');
      return;
    }
    emulatorRuntime.filterSearch(filter, parsedValue);
    refreshCandidates();
  };

  const freezeCandidate = (candidate: SearchCandidate) => {
    const formattedAddress = formatAddress(candidate.address);
    emulatorRuntime.addCheat(formattedAddress, candidate.address, candidate.value, width);
    onNotice(`Freeze added: ${formattedAddress} = ${candidate.value}`);
  };

  const applyCode = (freeze: boolean) => {
    if (!requireRom()) return;
    const parsedAddress = parseNumber(address);
    const parsedValue = parseNumber(value);
    if (Number.isNaN(parsedAddress) || Number.isNaN(parsedValue)) {
      onNotice('Enter a valid address and value.', 'error');
      return;
    }

    if (freeze) {
      const codeName = name.trim() || 'Code';
      emulatorRuntime.addCheat(codeName, parsedAddress, parsedValue, codeWidth);
      onNotice(`Freeze added: ${codeName}`);
      refreshCodes();
    } else {
      emulatorRuntime.applyCheat(parsedAddress, parsedValue, codeWidth);
      onNotice(`Applied: ${formatAddress(parsedAddress)} = ${parsedValue}`);
    }
  };

  return (
    <Modal open={open} title="Cheat Engine" className="cheat-modal" onClose={onClose}>
      <div class="dialog-tabs" role="tablist" aria-label="Cheat tools">
        <button class={tab === 'search' ? 'active' : ''} type="button" role="tab" onClick={() => switchTab('search')}>
          Memory search
        </button>
        <button class={tab === 'codes' ? 'active' : ''} type="button" role="tab" onClick={() => switchTab('codes')}>
          Cheat codes
        </button>
      </div>

      {tab === 'search' ? (
        <div class="cheat-panel">
          <div class="control-stack">
            <div class="control-row">
              <span class="field-label">Width</span>
              <div class="segmented-control" aria-label="Search value width">
                {[1, 2, 4].map((option) => (
                  <button
                    class={width === option ? 'active' : ''}
                    type="button"
                    onClick={() => setWidth(option)}
                  >U{option * 8}</button>
                ))}
              </div>
              <button class="button primary" type="button" onClick={resetSearch}>
                <RefreshCw aria-hidden="true" /> Reset search
              </button>
            </div>
            <div class="control-row">
              <select value={filter} onChange={(event) => setFilter(event.currentTarget.value as SearchFilter)} aria-label="Search filter">
                <option value="eq">Equal to</option>
                <option value="ne">Not equal to</option>
                <option value="gt">Greater than previous</option>
                <option value="lt">Less than previous</option>
                <option value="changed">Changed</option>
                <option value="unchanged">Unchanged</option>
              </select>
              {(filter === 'eq' || filter === 'ne') && (
                <input
                  value={filterValue}
                  onInput={(event) => setFilterValue(event.currentTarget.value)}
                  placeholder="100 or 0x64"
                  aria-label="Filter value"
                />
              )}
              <button class="button" type="button" onClick={applyFilter}>Filter</button>
              <button class="icon-button" type="button" onClick={refreshCandidates} title="Refresh values" aria-label="Refresh values">
                <RefreshCw aria-hidden="true" />
              </button>
            </div>
            <p class={`search-summary${searchInitialized ? ' active' : ''}`}>
              {!romLoaded
                ? 'Load a game first.'
                : searchInitialized
                  ? `${candidateCount} candidate${candidateCount === 1 ? '' : 's'}`
                  : 'Reset the search to begin.'}
            </p>
          </div>

          <div class="candidate-list">
            {!searchInitialized ? (
              <div class="empty-state">Search results will appear here.</div>
            ) : candidates.length === 0 ? (
              <div class="empty-state">No candidates remain.</div>
            ) : (
              <>
                {candidates.map((candidate) => (
                  <div class="candidate-row" key={candidate.address}>
                    <code>{formatAddress(candidate.address)}</code>
                    <span>{candidate.value}</span>
                    <button class="button" type="button" onClick={() => freezeCandidate(candidate)}>
                      <Snowflake aria-hidden="true" /> Freeze
                    </button>
                  </div>
                ))}
                {candidateCount > MAX_CANDIDATES && (
                  <p class="result-note">Showing the first {MAX_CANDIDATES} results. Narrow the search to see more.</p>
                )}
              </>
            )}
          </div>
        </div>
      ) : (
        <div class="cheat-panel">
          <div class="code-form control-row">
            <input value={name} onInput={(event) => setName(event.currentTarget.value)} placeholder="Name" aria-label="Code name" />
            <input value={address} onInput={(event) => setAddress(event.currentTarget.value)} placeholder="0x02001234" aria-label="Address" />
            <input value={value} onInput={(event) => setValue(event.currentTarget.value)} placeholder="Value" aria-label="Value" />
            <select value={codeWidth} onChange={(event) => setCodeWidth(Number(event.currentTarget.value))} aria-label="Code width">
              <option value="1">U8</option>
              <option value="2">U16</option>
              <option value="4">U32</option>
            </select>
            <button class="button" type="button" onClick={() => applyCode(false)}>Apply once</button>
            <button class="button primary" type="button" onClick={() => applyCode(true)}>
              <Plus aria-hidden="true" /> Add freeze
            </button>
          </div>
          <div class="code-list">
            {codes.length === 0 ? (
              <div class="empty-state">No cheat codes added.</div>
            ) : codes.map((code, index) => (
              <div class={`code-row${code.enabled ? '' : ' disabled'}`} key={`${code.address}-${index}`}>
                <strong title={code.name}>{code.name}</strong>
                <code>{formatAddress(code.address)} = {code.value} (U{code.width * 8})</code>
                <button class="button compact" type="button" onClick={() => {
                  emulatorRuntime.toggleCheat(index);
                  refreshCodes();
                }}>{code.enabled ? 'On' : 'Off'}</button>
                <button class="icon-button danger" type="button" aria-label={`Remove ${code.name}`} onClick={() => {
                  emulatorRuntime.removeCheat(index);
                  refreshCodes();
                }}>
                  <Trash2 aria-hidden="true" />
                </button>
              </div>
            ))}
          </div>
        </div>
      )}
    </Modal>
  );
}

function parseNumber(value: string): number {
  const trimmed = value.trim();
  if (!trimmed) return Number.NaN;
  return Number.parseInt(trimmed, trimmed.toLowerCase().startsWith('0x') ? 16 : 10);
}

function formatAddress(address: number): string {
  return `0x${address.toString(16).toUpperCase().padStart(8, '0')}`;
}

function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}
