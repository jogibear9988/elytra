# Public TLS test fixtures

These certificates and server key are public test data. Never trust the test CA or
use the server key in an application. The CA private key is not distributed.

The RSA-2048 leaf has serverAuth usage, DNS SANs localhost and *.elytra.test and IP
SANs 127.0.0.1/::1. It is signed by the test CA; clients must explicitly supply ca.pem.
The ordinary leaf expires in November 2036 and must be refreshed before then.
expired.pem is deliberately valid only in 2020; selfsigned.pem is an untrusted
self-signed leaf with the same server key. No third-party certificate/key data is
included. The CA private key is retained only in ignored local build scratch.
