Returns all transactions sent or received by the period-separated Bitcoin
addresses in parameter 1. The optional parameter 2 contains a hexadecimal block
hash: transactions in blocks up to and including this block will not be
returned.

The transactions are returned as a JSON object. The object's "keys" are transaction
hashes. The structure is like this (mostly the same as jgarzik's getblock):

- root
- transaction hash
- hash (same as above)
- version
- number of inputs
- number of outputs
- lock time
- size (bytes)
- inputs
- previous output
- hash of previous transaction
- index of previous output
- scriptsig (replaced by "coinbase" on generation inputs)
- sequence (only when the sequence is non-default)
- address (on address transactions only!)
- outputs
- value
- scriptpubkey
- address (on address transactions only!)
- block hash
- block number
- block time

Only transactions to or from the listed *addresses* will be shown. Public key
transactions will not be included.

When encountering an error, the response will start with "ERROR:", followed by
the error. An appropriate HTTP response code will also be sent. A response with
no body must also be considered to be an error.

/q/mytransactions/address1.address2/blockHash
