Generates a hash160 from a Bitcoin public key. In the current implementation,
public keys are either the first 65 bytes (130 hex characters) of a
scriptPubKey or the last 65 bytes of a scriptSig, depending on the type of
transaction. They always seem to start with 04 (this must be included).

/q/hashpubkey/hexPubKey
