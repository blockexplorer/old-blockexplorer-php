Returns 00 if the address is valid, something else otherwise. Note that it is
impossible to determine whether someone actually *owns* the address. Someone
could easily give 20 random bytes to /q/hashtoaddress and get a valid address.

X5 - Address not base58
SZ - Address not the correct size
CK - Failed hash check

Anything else - the encoded AddressVersion (always 00 in valid addresses)

/q/checkaddress/address
