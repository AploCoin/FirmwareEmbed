# Restricted mining sessions

The device never needs the owner private key. The owner is the funding and reward account; the session key is the transaction signer and has its own pending transaction nonce. Miner parameters bind to the session signer. Balances, staking and reward multiplier are queried for the owner. Do not fund the fresh session address: registration requires a previously unused, empty account.

## Issue a session on the owner-controlled machine

1. Select a compatible AploNode RPC exposing the native session registry `0x0000000000000000000000000000000000001237`. Read the effective `eth_chainId`; the proof and transaction must use that exact domain, including alternate signing domains. Never substitute a hardcoded library chain ID.
2. Generate a fresh session key with a CSPRNG. Keep only that restricted key available for provisioning the device. Do not put either private key in firmware source, build flags, configuration files, shell history or reports.
3. Define the policy: the mining target `APLO_MINING_CONTRACT` (`0x0000000000000000000000000000000000001234`), only the four-byte selector `0x2fdc505e` of `mine(bytes32)`, zero APLO value allowance, a finite GAPLO gas allowance in exact base units and an absolute expiry block. The current node limits lifetime to at most 1,000 blocks from inclusion. Select a shorter expiry that leaves room for owner transaction confirmation; the expiry block is inclusive.
4. The session key signs the library-generated acceptance proof for the owner, session, target, ordered selector list, budgets, expiry and effective chain ID. The owner signs a normal transaction to the registry calling `CreateSessionKey` with this proof. Use the owner's pending nonce and sufficient registration gas. AploEmbed provides `SessionKeys::AcceptanceProof(policy, owner, chainId, sessionKey, proof)`, `PrepareCreate(policy, owner, proof, gasLimit, tx)` and `SignTransaction(tx, ownerDigestSigner, context, owner, raw)`. The signer callback may live on a separate signing device. Use its `examples/Aplo Session Keys` demonstration for canonical encoding; do not reimplement ABI or hash logic in this repository.
5. Wait for the owner transaction receipt. Broadcast acceptance alone does not authorize the device. Verify the registry record matches the intended owner, target, allowances and expiry. Keep public authorization details (chain ID, owner, session address, expiry and transaction hash) for recovery; these are not secrets.
6. Choose `session` in the [USB configurator](serial-api.md). Provide the approved owner, session signer, chain ID, maximum GAPLO allowance and absolute expiry. Import the corresponding session key through the write-only key field and commit. The firmware checks that the key derives the expected session address.

## Lifecycle and recovery

Provision an explicit PEM CA for each HTTPS RPC endpoint and set reachable NTP servers. ESP8266 endpoints must also negotiate 1024-byte MFLN. A TLS or time failure pauses the affected RPC.

The firmware checks the registered scope and remaining budget before spending gas. The gas allowance must cover the maximum fee reservation (`gasLimit * gasPrice`), although actual included fees reduce the remaining allowance. The owner also needs enough GAPLO; the allowance does not deposit funds. Zero APLO allowance is intentional for mining, whose native transaction value is zero. Session authorization does not stake funds or grant arbitrary contract access.

If the next inclusion block would exceed expiry, stop submitting. If the record is absent, it may have been revoked or removed by expiry cleanup; `getSession` alone cannot distinguish these. Report that uncertainty rather than guessing. On budget exhaustion or insufficient maximum-fee allowance, stop and obtain a fresh authorization with an appropriate budget. RPC errors pause work; they do not prove revocation or budget exhaustion.

The owner can use `SessionKeys::PrepareRevoke(sessionAddress, owner, gasLimit, tx)` to call `RevokeSessionKey(sessionAddress)` from the owner's signing device and wait for its receipt. Revocation cannot erase an already included transaction and may race an in-flight submission. On disconnect or pending receipt timeout, retain the transaction hash and keep checking it; do not declare success or automatically submit duplicate work. Only receipt `status=1` is success; `status=0` is an executed revert. Transport/RPC errors are distinct from a valid null receipt (pending).

The owner must register a fresh key and acceptance proof after expiry, revocation or budget exhaustion. Rebooting retains the signing key and the existing expiry and allowance.

## Direct Private Key

Choose `direct` and import the key of the funded, staked account. Its derived address is both the mining identity and the ordinary transaction sender. This mode sends a zero-value EIP-155 transaction using the account's ordinary nonce. It requires the canonical mining stake and enough GAPLO for the maximum fee. Use an account reserved for the device so another signer does not occupy its nonce concurrently.

## Power loss and outstanding shares

The device writes the transaction hash, nonce, proof, fee and exact signed bytes to flash before attempting a broadcast. On reboot it checks the stored signature and transaction fields, queries the receipt by hash and reconciles the latest and pending nonce. A valid receipt clears the record durably and distinguishes a confirmed share from a revert.

If the RPC returns a null receipt after restart, the miner may resend the identical bytes once when authorization, nonce, proof and current fees still permit it. Inconclusive nonce movement, missing RPC evidence, expiry and revocation can leave the record pending. Check the public journal hash on the owner machine. A new signature cannot replace an unresolved record automatically. Use the authenticated reset procedure only after reconciling the old transaction independently.

These rules follow `AploNode/builtin/sessionkeys/registry.go`, `core/state_transition.go` and WebMiner's `src/features/webminer/session-keys/protocol.ts`. Hardware and live-network behavior must be validated separately from compile-time checks.
