# Network to Monitor is session-only

Network to Monitor starts at All NICs on every launch and is not written to the registry or a configuration file. If the selected NIC disappears during the session, selection falls back to All NICs; an available but disconnected selected NIC remains selected and reports zero rates.

Persisting a machine-specific interface identifier can restore a stale or unavailable choice after hardware, driver, or network-profile changes. Session-only selection keeps startup deterministic while the operator settings in ADR-0007 persist independently.
