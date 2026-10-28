# Playerbots — portable integration

This fork supplies bot companions and autonomous random bots for
[AzerothCore Playerbots Portable](https://github.com/Syntax555/mod-playerbots-portable).
It includes native Playerbots class AI and Raid Clear tactics, with the assembly's
earned progression, auction trading, combat profiles and battleground gates.

## Install or build

For normal use, download the Windows server and matching EraTalents client pack
from the assembly's [Latest release](https://github.com/Syntax555/mod-playerbots-portable/releases/latest)
and follow its [quick start](https://github.com/Syntax555/mod-playerbots-portable#quick-start).
Optional [MultiBot and other client addons](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/addons.md)
are published by the maintained addon forks.

For source builds, follow the assembly's
[build guide](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/building.md).
Use the exact core and module commits in
[versions.lock.json](https://github.com/Syntax555/mod-playerbots-portable/blob/main/versions.lock.json).
This fork develops on `evermore` and requires the assembly's Playerbots core;
the standard AzerothCore core and unrelated module heads are not verified
replacements. The complete native build runs in the assembly.

## Behavior and configuration

With the portable profile, random bots begin at level 1 with normal starter
items and earn XP, gold, gear and each expansion unlock. They travel with normal
navigation and discovered travel access, pay trainers and vendors, and trade
owned items through ordinary auctions and mail. Grant, summon and recovery
shortcuts guarded by natural progression are refused. Existing inventories and
gold are preserved without checking their origin.

Humans, SelfBots and controlled altbots retain human character rights; their
talents remain manually controlled. Automatic talent spending applies to random
bots' earned points. Combat profiles reuse native class strategies and score
usable equipment already in the character's bags.

The portable configuration is `configs/modules/playerbots.conf`. Its managed
profile enables `Playerbots.NaturalProgression`, `Playerbots.EarnedAuctions` and
`Playerbots.EarnedEraBattlegrounds`; this repository's
[configuration template](conf/playerbots.conf.dist) leaves those opt-in flags off.
Use the assembly's [configuration guide](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/configuration.md),
[combat profiles](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/combat-profiles.md),
[auction guide](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/earned-auctions.md)
and [PvP guide](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/earned-bot-brackets.md).

Navigation, quests, professions and encounters depend on existing AI support.
Some upstream encounter actions still use movement or combat shortcuts.
Autonomous raids, bot arenas and bot Strand of the Ancients are unavailable in
the portable profile. Source fixtures do not establish full dungeon/raid runs,
every class's live behavior or performance at the configured 2,500-bot target.
See [dungeons and raids](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/bot-dungeons.md)
and [outstanding validation](https://github.com/Syntax555/mod-playerbots-portable/blob/main/docs/todo.md).

## Development and provenance

Read [AGENTS.md](AGENTS.md) and the assembly's
[contributing guide](https://github.com/Syntax555/mod-playerbots-portable/blob/main/CONTRIBUTING.md).
Make validated changes on `evermore`, then pin the commit in the assembly.
Preserve earned-play gates, user data, configuration and pinned history.

Imported from [buildthehomelab/wow-mod-playerbots at `5283a13e0a5d`](https://github.com/buildthehomelab/wow-mod-playerbots/commit/5283a13e0a5d873350fdaef777b95d553d354bee).
The [original README](https://github.com/buildthehomelab/wow-mod-playerbots/blob/5283a13e0a5d873350fdaef777b95d553d354bee/README.md)
and [Playerbots command wiki](https://github.com/mod-playerbots/mod-playerbots/wiki/Playerbot-Commands)
provide general references; some documented shortcuts are intentionally refused
in this assembly.

Playerbots builds on IKE3's Playerbots, ZhengPeiRu21/mod-playerbots,
celguar/mangosbot-bots, mod-playerbots contributors and the imported fork's
authors. Preserve the [GPL-2.0 license](LICENSE) and individual source notices,
including notices permitting later GPL versions.
