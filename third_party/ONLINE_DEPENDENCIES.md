# Online dependency source and notices

The optional `datachannel.dll` runtime is built from the following exact source.
`tools/BuildOnlineDeps.ps1` verifies the checkout commits and submodule revisions.
The checked-in C API and JSON headers retain their upstream licenses.

| Component | Pinned source | License |
| --- | --- | --- |
| libdatachannel 0.24.5 | [443f6934d9007eb7076ab7825ba330f355fcbead](https://github.com/paullouisageneau/libdatachannel/tree/443f6934d9007eb7076ab7825ba330f355fcbead) | MPL 2.0 |
| Mbed TLS 3.6.7 | [068ff080b369adfac81509f9b57b2afabaf82dc5](https://github.com/Mbed-TLS/mbedtls/tree/068ff080b369adfac81509f9b57b2afabaf82dc5) | Apache 2.0 OR GPL 2.0 or later; distributed under Apache 2.0 |
| libjuice | 3c40a3545b6b1b62c7adee7f8f2bd58aa290afd6 | MPL 2.0 |
| usrsctp | fec583d54493f879d2ae44a743423bf8a04371ab | BSD 3-Clause |
| plog | 94899e0b926ac1b0f4750bfbd495167b4a6ae9ef | MIT |
| nlohmann/json | 55f93686c01528224f448c19128836e7df245f72 | MIT |

Mbed TLS's bundled Everest and p256-m notices are retained in its upstream
`LICENSE` and accompanying source license files. The build uses unmodified source,
static TLS/SCTP/ICE dependencies, `NO_MEDIA=ON`, `NO_WEBSOCKET=ON` and enables
`MBEDTLS_SSL_DTLS_SRTP`, required by the pinned libdatachannel TLS implementation.
It does not configure a TURN server. `libjuice` contains TURN support upstream,
but the game only accepts STUN URLs and never provisions or uses TURN credentials.

The DLL still requires the Microsoft Visual C++ runtime corresponding to the
build architecture. Include `third_party/online-licenses/` and these source links
when distributing it. Source remains available at the pinned links and the build
script reproduces it; local upstream source modifications must retain MPL notices
and make the corresponding modified MPL source available when distributed.
