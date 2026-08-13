# No settings persistence and no autostart in v1

Every launch resets to defaults: NIC Selection is All NICs, no INI/registry product settings, and Win Mon does not register itself for Start with Windows.

v1 is a minimal tray utility; persistence and autostart add install-surface and failure modes without being required for the Rate Strip demo. Users can start the process manually or add a shortcut themselves. Revisit if remembered NIC or logon start becomes a product requirement.

Superseded in part by ADR-0007: operator toggles (Launch at Login, Right-Click Speed Text — originally shipped as "Rate Strip Right-Click Menu") now persist under `HKCU\Software\Win Mon` and the Run key. The rest of this decision stands — the network choice, since relabelled Network to Monitor, is not persisted and every launch starts from defaults.
