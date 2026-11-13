## Threat model (shot out to dre)

If the attacker can: control the machine enough to invoke enclave then:
  1. They can generate a valid 6-digit code by invoking the enclave for
     current time t.
  2. Worse: Because the enclave accepts a time value chosen by the caller
     they can generate an stockpile of future autentication (6-digit)
     codes for chosen future times.  They can then offload (exfiltrate)
     this stockpile from the device, limited by their access duration
     and the chip’s speed.

Then:
  - If they also obtain access to my password they can  use those codes
    to log in during the corresponding time windows, even after losing
    machine access, provided the authenticator’s secret has not changed

Assume login requires both valid code and password.  Then:
 1. If they have valid code but submit an incorrect password the password
    check blocks the login.
 2. If they have the correct password but submit an invalid code, the
    authenticator check blocks the login.  Note: space of guesses bounded
    by 10^6 so without attempt limits, this is likely more feasible.

## Then what does this protect against?
- Limit of explosion radius ([pp. 7, dabo](https://cs255.stanford.edu/lectures/key-exch.pdf))
- Forward secrecy



## Can we do even better??
