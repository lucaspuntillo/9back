## 9BACK, Looks like the front fell off too!
9BACK, is a Unix-like OS and is a fork of 9Front, which in turn is also a fork of Plan 9 by Bell Labs. 

**But why?**

I think Plan 9 is a really cool OS, and I decided to modify it to my needs.

**Licence**
9Front is GPL licenced, so that means 9BACK needs to be GPL licenced too.

**Where can I get the original code?**
Right [here.](https://code.9front.org/hg/plan9front/)

**What is the basis of this OS?**

> Plan 9 really pushes hard on some ideas that Unix has that haven’t really been fully developed, in particular, the notion that just about everything in the system is accessible through a file. In other words, things look like an ordinary disk file. So all the devices are controlled this way by means of ASCII strings, not complicated data structures. For example, you make network calls by writing an ASCII string, not the files. This notion is something that’s actually leaking quite fast.
The second thing is sort of more subtle and sort of hard to appreciate until you’ve actually played with it. That is that the set of files an individual program can see depends on that program itself. In a standard kind of system, either with Unix remote file systems or Windows attached file systems, all the programs running in the machine see the same thing. In Plan 9, that’s adjustable per program. You can set up specialized name stations that are unique to a particular program. I mean, it’s not associated with the program itself but with the process, with the execution of the process.

>— Dennis Ritchie (Creator of the UNIX Operating System)

(Really it's the basis of Plan 9, but it applies here too.)

**Important links:**
[9Front](http://9front.org)
[9Front codebase](https://code.9front.org/hg/plan9front)
