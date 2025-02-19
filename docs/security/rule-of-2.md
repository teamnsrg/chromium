# The Rule Of 2

When you write code to parse, evaluate, or otherwise handle untrustworthy inputs
from the Internet — which is almost everything we do in a web browser! — we like
to follow a simple rule to make sure it's safe enough to do so. The Rule Of 2
is: Pick no more than 2 of

  * untrustworthy inputs;
  * unsafe implementation language; and
  * high privilege.

## Why?

When code that handles untrustworthy inputs at high privilege has bugs, the
resulting vulnerabilities are typically of Critical or High severity. (See our
[Severity Guidelines](severity-guidelines.md).) We'd love to reduce the severity
of such bugs by reducing the amount of damage they can do (lowering their
privilege), avoiding the classes of memory corruption bugs (using a safe
language), or reducing the likelihood that the input is malicious (asserting the
trustworthiness of the source).

## What?

_Untrustworthy inputs_ are inputs that

  * have non-trivial grammars; or
  * come from untrustworthy sources.

Unfortunately, it is very rare to find a grammar trivial enough that we can
trust ourselves to parse it successfully or fail safely. (But see
[Normalization](#Normalization) for a potential example.)

Obviously, any arbitrary peer on the Internet is an untrustworthy source without
some evidence of trustworthiness (which includes at least [a strong assertion of
the source's identity](#verifying-the-trustworthiness-of-a-source)).

_Unsafe implementation languages_ are languages that lack
[memory safety](https://en.wikipedia.org/wiki/Memory_safety), including at least
C, C++, and assembly language. Memory-safe languages include Go, Rust, Python,
Java, JavaScript, Kotlin, and Swift.

_High privilege_ is a relative term. The very highest-privilege programs are the
computer's firmware, the bootloader, the kernel, any hypervisor or virtual
machine monitor, and so on. Below that are processes that run as an OS-level
account representing a person; this includes the Chrome browser process. We
consider such processes to have high privilege. (After all, they can do anything
the person can do, with any and all of the person's valuable data and accounts.)

Processes with slightly reduced privilege include (as of January 2019) the
network process and the GPU process. These are still pretty high-privilege
processes. We are always looking for ways to reduce their privilege without
breaking them.

Low-privilege processes include sandboxed utility processes and renderer
processes with [Site
Isolation](https://www.chromium.org/Home/chromium-security/site-isolation) (very
good) or [origin
isolation](https://www.chromium.org/administrators/policy-list-3#IsolateOrigins)
(even better).

## Solutions To This Puzzle

Chrome Security Team will generally not approve for landing a CL or new feature
that involves all 3 of untrustworthy inputs, unsafe language, and high
privilege. To solve this problem, you need to get rid of at least 1 of those 3
things. Here are some ways to do that.

### Privilege Reduction

Also known as [_sandboxing_](https://cs.chromium.org/chromium/src/sandbox/),
privilege reduction means running the code in a process that has had some or
many of its privileges revoked.

When appropriate, try to handle the inputs in a renderer process that is Site
Isolated to the same site as the inputs come from. Take care to validate the
parsed (processed) inputs in the browser, since the semantics of the data are
not necessarily trustworthy yet.

Equivalently, you can launch a sandboxed utility process to handle the data, and
return a well-formed response back to the caller in an IPC message. An example
of launching a utility process to parse an untrustworthy input is [Safe
Browsing's ZIP
analyzer](https://cs.chromium.org/chromium/src/chrome/common/safe_browsing/zip_analyzer.h).

### Verifying The Trustworthiness Of A Source

If you can be sure that the input comes from a trustworthy source, it can be OK
to parse/evaluate it at high privilege in an unsafe language. A "trustworthy
source" meets all of these criteria:

  * communication happens via validly-authenticated TLS, HTTPS, or QUIC;
  * peer's keys are [pinned in Chrome](https://cs.chromium.org/chromium/src/net/http/transport_security_state_static.json?sq=package:chromium&g=0); and
  * peer is operated by a business entity that Chrome should trust (e.g. an [Alphabet](https://abc.xyz) company).

### Normalization {#normalization}

You can 'defang' a potentially-malicious input by transforming it into a
_normal_ or _minimal_ form, usually by first transforming it into a format with
a simpler grammar. We say that all data, file, and wire formats are defined by a
_grammar_, even if that grammar is implicit or only partially-specified (as is
so often the case). A file format with a particularly simple grammar is
[Farbfeld](https://tools.suckless.org/farbfeld/) (see the table at the top).
It's rare to find such a simple grammar, however.

For example, consider the PNG image format, which is complex and whose [C
implementation has suffered from memory corruption bugs in the
past](https://www.cvedetails.com/vulnerability-list/vendor_id-7294/Libpng.html).
An attacker could craft a malicious PNG to trigger such a bug. But if you
transform the image into a format that doesn't have PNG's complexity (in a
low-privilege process, of course), the malicious nature of the PNG 'should' be
eliminated and then safe for parsing at a higher privilege level. Even if the
attacker manages to compromise the low-privilege process with a malicious PNG,
the high-privilege process will only parse the compromised process' output with
a simple, plausibly-safe parser. If that parse is successful, the
higher-privilege process can then optionally further transform it into a
normalized, minimal form (such as to save space). Otherwise, the parse can fail
safely, without memory corruption.

The trick of this technique lies in finding a sufficiently-trivial grammar, and
committing to its limitations.

Another good approach is to define a Mojo message type for the information you
want, extract that information from a complex input object in a sandboxed
process, and then send the information to a higher-privileged process in a Mojo
message using the message type. That way, the higher-privileged process need
only process objects adhering to a well-defined, generally low-complexity
grammar. This is a big part of why [we like for Mojo messages to use structured
types](mojo.md#Use-structured-types).

### Safe Languages

Where possible, it's great to use a memory-safe language. Of the currently
approved set of implementation languages in Chromium, the most likely candidates
are Java (on Android only) and JavaScript (although we don't currently use it in
high-privilege processes like the browser). One can imagine Swift on iOS or
Kotlin on Android, too, although they are not currently used in Chromium.

## Existing Code That Violates The Rule

Obviously, we still have a lot of code that violates this rule. For example,
until very recently, all of the network stack was in the browser process, and
its whole job is to parse complex and untrustworthy inputs (TLS, QUIC, HTTP,
DNS, X.509, and more). This dangerous combination is why bugs in that area of
code are often of Critical severity:

  * [OOB Write in `QuicStreamSequencerBuffer::OnStreamData`](https://bugs.chromium.org/p/chromium/issues/detail?id=778505)
  * [Stack Buffer Overflow in `QuicClientPromisedInfo::OnPromiseHeaders`](https://bugs.chromium.org/p/chromium/issues/detail?id=777728)

We now have the network stack in its own dedicated process, and have begun the
process of reducing that process' privilege. ([macOS
bug](https://bugs.chromium.org/p/chromium/issues/detail?id=915910), [Windows
bug](https://bugs.chromium.org/p/chromium/issues/detail?id=841001))
