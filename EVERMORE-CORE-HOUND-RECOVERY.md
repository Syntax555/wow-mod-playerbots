# Molten Core hound recovery

Enable `Playerbots.McCoreHoundRecovery = 1` (realm compose override:
`AC_PLAYERBOTS_MC_CORE_HOUND_RECOVERY: "1"`) after deploying this module.
The default is disabled for upstream compatibility.

Hounds with Play Dead (19822) remain alive but take no damage. When an engaged
hound is still awake and another nearby smoldering hound has at most three
seconds left, bots pause new damage and wait until the smoldering hounds have
revived. They then resume the existing highest-health focus strategy. A pack
that reaches the all-smoldering state can die normally without a recovery pause.
The scan follows the core mechanic's 80-yard and line-of-sight checks, runs once
per second per bot, and is gated to combat in Molten Core. Pets temporarily use
passive stance; healing and escape movement remain available. Existing damage
over time, summoned guardians and attacks from human players are not removed.

Validation: repository codestyle and changed-line formatting checks; independent
source review against realm core e3cdb38576c4e42a408d16ed6f2c60fce3d71685.
No server build, game-client test or pmon measurement has been performed.
Test with melee, hunter Auto Shot, wand users and pets: miss a pack kill window,
confirm damage pauses, healing continues, all smoldering hounds revive, damage
resumes and the pet's previous stance returns. Also test a normal synchronized
kill, a wipe, two nearby packs and leaving combat during recovery.
