# Change Log

## 2026-09-24 - Fixes from a code audit

**Impact:** fixes: unloading while the taskbar does not answer, an icon's
version after it is added again, the arrange window while icons come and go,
a picture taken away in the embedded tray, and the tooltip setting.

A code audit of `817033a` reported five findings. It reproduced three against
the shipped source through this project's own harness, and traced the other
two, in the XAML half, through the source. Each was checked against the code
here and is now a regression test. None had been seen live; three of the fixes
were checked live afterwards, the last items under Verified.

* **Unloading could wait for ever for a taskbar that did not answer**
  (DECISIONS 73). `HandBackToShell` asked for the hand-back with
  `SendMessageW` before its five-second wait began, and `RemoveEmbeddedTrays`
  did the same, so a taskbar thread that was blocked held unloading for as
  long as it was. Taking the subclass off from the unloading thread is a
  message that thread has to answer too. Now:
  * The panels' removal and the hand-back are sent with a time limit
    (`SendToTaskbarBy`, `g_taskbarWaitMs`). One not answered in time is kept
    for the thread to handle when it gets to it, and the module stays loaded
    for that code.
  * Found by the new test: a sent message that times out before it is
    handled is dropped, not handled later. The hand-back never came, and the
    subclass went on swallowing icons into trays nothing drew. One not
    answered in time is posted as well.
  * The subclass takes itself off, on the taskbar's thread, in the call that
    finishes the hand-back.
  * Unloading waits until no call of the subclass is under way on that thread
    (`g_subclassDepth`), and then for one more message to be answered there.
    Explorer may run a message loop inside a message the subclass passed on,
    a menu say, and the hand-back is then done inside that loop, with a call
    of the subclass still below it. Unloading used to go ahead there.
* **An icon added again kept its old version** (DECISIONS 74). An icon
  registered again by GUID from a new window, as when its application
  restarts, kept the version the old registration had asked for. Clicks in
  Split Tray's trays were packed for version 4 to an application expecting
  version 0, and a move into Explorer's tray replayed that version. An add the
  mod answers itself now starts the icon at version 0. So does one Explorer
  takes, while one it refuses because it has the icon leaves the version as it
  was, as Explorer does.
* **An open arrange window missed icons coming and going** (DECISIONS 75). It
  was filled when it opened and again only after a move made in it. An icon of
  the main tray asked for no refresh at all, and a refresh with the same trays
  did nothing. Now:
  * Each refresh compares which icons there are, the tray each is in and
    whether it is in that tray's overflow (`ArrangeLayoutNow`) with what the
    lists were filled with.
  * An icon of the main tray coming or going asks for a refresh, and so does
    the watchdog dropping an icon whose application went.
  * Pictures and tooltips are left out, as before, since they change several
    times a second.
  * What is selected stays selected, by key.
  * A refresh in the middle of a drag is left to the drag's end.
* **A picture taken away stayed in the embedded tray** (DECISIONS 76).
  `UpdateCellInPlace` set a picture only when there was one, so an application
  that took its icon's picture away left the old one showing until something
  else rebuilt the tray. A snapshot now says whether the store has a picture
  (`CellSnapshot::hasPicture`). `CellPictureOf` tells a picture taken away
  from one that could not be copied for this refresh; the store still has
  that one (DECISIONS 72), and the cell keeps showing it.
* **"Show tooltips" was not applied everywhere** (DECISIONS 77).
  * The cells in a taskbar, and in its overflow popup, had their icon's
    tooltip whatever the setting said. `CellSnapshotsOf` now leaves it out
    when tooltips are off.
  * A floating tray read the setting only when its window was made.
    `SyncFloatingTrays` now makes or removes each tray's tooltip window on
    every pass.
  * The chevron's and the empty tray's own labels belong to the mod's
    controls, not to icons, and are kept.
* `tools/mutants_reviews.py`: two mutants follow code they described that
  changed. The audit's mutants are in `tools/mutants_audit.py`, so that no
  module passes 400 lines.

**Verified:**

* Regression 879/0. New tests:
  * `Test_UnloadingWaitsForATaskbarThatDoesNotAnswerOnlyItsTime`, on a
    taskbar thread of the test's own held on an event, with the real subclass
    on its window
  * `Test_UnloadingEndsOnlyOnceTheModsCodeHasLeftTheTaskbarsThread`, with the
    hand-back done inside a message loop run under the subclass
  * `Test_AnAddThatIsTakenStartsTheIconAtVersionZero`
  * `Test_TheArrangeWindowsRowsFollowWhichIconsThereAreAndWhere`
  * `Test_AnEmbeddedCellShowsThePictureTheStoreHas`
  * `Test_AnEmbeddedTraysTooltipsFollowTheSetting`

  All six failed before their fixes, with 11 failing checks. Nothing else
  failed. The second was then rewritten to run the hand-back inside a real
  message loop, since setting the count by hand let a mutant through; that
  version was not run against the old code. The first then failed once, a
  race in the test itself: unloading's message reached the test's taskbar
  thread before the message that holds it. It now waits until the thread
  is held.
* Integration 285/0. New and changed phases, all of which failed before the
  fixes, with 5 failing checks:
  * [11g]: with the arrange window open, an icon for the main tray comes and
    goes. Its row comes and goes, a row selected in tray 2's list stays
    selected, and a change in the middle of a drag waits for the drag to end.
  * [11h]: switching tooltips off takes tray 2's tooltip window away, and
    switching them on puts it back.
  * [12]: the subclass is taken off exactly once as the mod unloads.
  * [18]: a hand-back not done in time leaves the subclass on the window. Once
    the round is over, the hand-back takes it off, and Explorer hears
    applications directly again.
* Twenty new mutants, in `tools/mutants_audit.py`, and two older ones updated
  for code that changed. Mutation score 110/110 = 100%, in two runs on the
  committed source: the first was stopped after 60 mutants, all killed, and
  the other 50, the audit's twenty among them, were run on their own
  afterwards, all killed. The only change to the tests in between was the
  wait for the test's taskbar thread to be held.
* The mod compiles clean with the Windhawk editor's flags, XAML half included.
  Windhawk's pull request validation reports no warnings. MSP fitness
  85.7/100.
* Live, in Explorer, with a test application whose icons are driven line by
  line (`st-blink`, a copy of it under another name for the main tray, and a
  rule sending `st-blink.exe` to tray 2 for the run):
  * **Installed in place**, then **switched off and on four times**. Each
    unload was over, and the mod's module gone from Explorer - not kept
    loaded - 0.67 to 0.97 seconds after it was asked for. After every unload
    SystemInformer's four icons, Telemachus and the test icon were with
    Explorer; after every load they were back in tray 2.
  * **A picture taken away**, from the test icon in tray 2, in the taskbar:
    screenshots of its cell had 171 pixels of the icon's blue with the
    picture, none once it was taken away, and 171 again once it was given
    back.
  * **The arrange window**, opened from outside with the controller's own
    message: its main tray list went from 20 rows to 21 and 22 as two test
    icons were added, and back to 21 and 20 as one was removed and the other's
    application quit, each within 0.6 seconds.
  * The first attempt at the arrange window never opened it: the script passed
    PowerShell's `$null` for the window title, which reaches a .NET string
    parameter as an empty string.

**Not verified:**

* Not live: the version of an icon added again, the tooltip setting, a
  taskbar thread that does not answer, and the tray 2 list of the arrange
  window. The regression and integration tests cover each.
* Both suites leave the XAML half out: the cell's picture and tooltip are
  tested through `CellPictureOf` and `CellSnapshotsOf`, and in a taskbar only
  live, for the picture. The panels' removal with a time limit is not
  tested.
* The last message answered once no call of the subclass is under way covers
  the instructions left of that call's return. No test can reach that window,
  and no mutant covers it.

## 2026-09-24 - Fixes from a third external review

**Impact:** fixes: unloading while applications carry on, attaching before the
tray thread runs, what Explorer's answers are taken to mean, and icon pictures
that cannot be copied.

The same reviewer went over the second review's fixes with 20 checks on
functions extracted from the source, Windows simulated. It confirmed all four
fixes, recommended keeping the settling design, and reproduced four remaining
defects and one conditional one. Each was checked against the code here and
is now a regression test. Two more changes came from testing in Explorer
afterwards; they are the last two items.

* **Unloading stopped keeping track before the icons were back**
  (DECISIONS 68). `Wh_ModBeforeUninit` set the unloading flag, and from then
  the subclass passed applications' messages to Explorer without updating the
  store. The icons were handed back later, in `Wh_ModUninit`, from that stale
  store. An application that removed an icon in between, while keeping its
  window, had it put back in Explorer's tray. One that changed its callback
  had the old one put back. Now:
  * The subclass keeps track until the hand-back, unloading or not.
  * The hand-back runs in `Wh_ModBeforeUninit` once the tray thread has
    stopped, on the taskbar's thread, and reads the store as it is then
    (`HandIconsBackToShell`). What arrives while it runs is handed back by
    another round.
  * In the same step the subclass starts passing everything on untouched, so
    no message falls between the hand-back and the subclass letting go.
  * A hand-back that arrives inside a round of settling is done when that round
    ends. Unloading waits for it, and one not done in time keeps the module
    loaded, as a tray thread that will not stop already did.
* **A tray thread slower than `Wh_ModInit`'s wait could still attach**
  (DECISIONS 69). The end of the wait was taken as leave to attach. A thread
  that then failed left a subclass swallowing icons into trays nothing drew.
  `SubclassShellTrayWindow` now refuses until the thread is running, and a
  slow thread attaches from its own timer. One that gives up afterwards
  leaves nothing attached, and logs that it gave up.
* **Explorer's answers were recorded only for an icon it had refused**
  (DECISIONS 70). An application's own add that Explorer refused was recorded
  as there, and nothing put that right. Every add or modify passed on to
  Explorer now has its answer recorded. A refused add is not yet taken as
  absence: Explorer refuses to add an icon it has already, which is every
  application's add when the mod is loaded into a running Explorer. The
  taskbar's next round asks with the same icon as a modify (`ProbeRecordFor`).
  An icon Explorer does not have is left to its application, which has been
  told, as with no mod.
* **Explorer could end up with an older icon than the store** (DECISIONS 71).
  * What Explorer answered the version sent after an add was not looked at.
    An icon whose version it refused was recorded as settled.
  * A message that arrived while Explorer was taking an icon back was
    swallowed, since Explorer did not have the icon yet, and never reached it.
    Explorer may send messages of its own while it handles a record, and they
    come back through the subclass. The reviewer injected this; it has not
    been seen in Explorer.
  * A wake-up dispatched inside a round of settling started a second round
    inside the first, which handed Explorer the same add again.

  Each icon now counts its changes. One that changed during its add, or whose
  version was refused, is marked as behind, and the next round hands Explorer
  the whole record as a modify with the version after it, three times at most.
  A round never starts inside another.
* **A picture that could not be copied** (DECISIONS 72). When the delivery's
  copy of the mod's own picture failed, the add went with the handle the
  application last sent, destroyed long before or by then another icon's.
  `AddRecordFor` now leaves the picture out instead, and Explorer adds the icon
  without one until its application sends one. The store also let go of an
  icon's picture before copying the new one, so a failed copy left it with
  none (`TakePictureLocked`).
* **Found in Explorer: the hand-back asked for icons the mod never took**
  (DECISIONS 68). When the mod is loaded into a running Explorer, Explorer's
  own icons - its volume icon, uID 100, among them - register again, and
  Explorer refuses both their add and the modify that asks about it. Recorded
  as not Explorer's, they were handed back at every unload, three times each,
  and refused each time. The hand-back now gives fresh attempts only to icons
  whose place it changes. The refusal lines in the log now name the
  application: the log gave only uIDs, and two applications in tray 2 both use
  uID 2.
* **Found in Explorer: asking about a refused add in the middle of the
  application's message** (DECISIONS 70, 51). The first version asked with the
  modify straight away. On a load into a running Explorer every application
  registers again at once, Explorer refuses nearly every add in that burst,
  and each refused add became two calls into Explorer instead of one. With
  the question asked there, a log listener attached and the processor busy
  with the mutation run, Telemachus's icon was lost in two loads of six.
  Telemachus is a Tauri application, which does not retry when the tray keeps
  it waiting.
  Whether the question was the cause was not measured. The question is now
  posted to the taskbar's next round, after the messages waiting then, and
  asks without the picture, whose handle may be gone by then.
* Two existing checks changed with the behaviour they described:
  `Test_AddRecordDrawsWithTheModsOwnPicture` expected a record with no picture
  of the mod's to keep the application's old handle. The end of
  `Test_AnAddExplorerRefusesIsAskedAgainThenLeftToItsApplication` expected an
  ordinary icon's answers not to be recorded.
* `tools/mutants.py`: two mutants follow code they described that moved.

**Verified:**

* Regression 847/0. New tests:
  * `Test_EveryIconIsHandedBackAsItIsWhenTheModUnloads`
  * `Test_TheHandBackLeavesAloneAnIconExplorerRefusedItsApplication`
  * `Test_AHandBackThatArrivesDuringARoundIsDoneAfterIt`
  * `Test_ASettlingRoundIsNotStartedInsideAnother`
  * `Test_WhatArrivesWhileExplorerTakesAnIconBackFollowsIt`
  * `Test_AVersionExplorerDoesNotTakeIsAskedForAgain`
  * `Test_ExplorersAnswerToAnApplicationsOwnMessageIsRecorded`
  * `Test_ARefusedAddIsAskedAboutWithAModifyOfTheSameIcon`
  * `Test_AnAddNeverCarriesAPictureTheModDoesNotOwn`
  * `Test_APictureThatCannotBeCopiedLeavesTheOneBefore`
  * `Test_TheModAttachesOnlyOnceItsTrayThreadRuns`

  Each failed before its fix. For the review's findings, that was 25 failing
  checks, the two changed checks included.
* Integration 265/0. The stand-in for Explorer now keeps each icon's tooltip.
  New phases and checks. All but [13c], which was added after the fixes,
  failed before them:
  * [11f]: an application's own add that the stand-in refuses is recorded as
    not there, and one refused because the stand-in has the icon as there.
    The question is asked after the application's message, not inside it.
  * [12]: with unloading begun, an application removes one icon and changes
    another. The removed one is not put back, and the changed one comes back
    changed.
  * [13c]: the test the reviewer asked for. The mod is switched off and on
    four times while an application adds, changes and removes icons with its
    window open, some of it after unloading has begun. After each unload the
    stand-in held exactly the application's icons, with their latest tooltips.
  * [16b]: a tray thread held until after `Wh_ModInit`'s wait, then made to
    fail. The mod was not attached while it started, nor after it gave up, and
    an icon for tray 2 went to the stand-in (`g_trayThreadHold`).
  * [18]: a hand-back that cannot finish keeps the mod loaded, with the
    subclass off the window.
* Twenty new mutants, in `tools/mutants_reviews.py`. Mutation score
  90/90 = 100%. The first run on the final code ran out of disk on C: and
  was run again with its temporary files on E:.
* The mod compiles clean with the Windhawk editor's flags, XAML half included.
  Windhawk's pull request validation reports no warnings. MSP fitness 85.7/100.
* Live, in Explorer, over three runs:
  * **After a restart:** the tray thread was running before the mod attached;
    the taskbar did not exist yet, and the timer attached. Moving Telemachus
    to tray 1 and back through the arrange window: Explorer took the add (the
    icon was then on the primary display) and the mod had it again after.
  * **The reviewer's test, live.** A test application kept three icons
    changing - one updated every 15 ms, one added and removed, one removed
    the moment the mod began to unload - with its window open, while the mod
    was switched off and on around it. In six unloads over two runs, Explorer
    held the constantly updated icon each time. It held the added-and-removed
    one exactly when that was last added, and never the one removed while
    unloading.
  * **Tray 2's other icons** (SystemInformer's four, Telemachus, the Claude
    usage monitor), asked for after each step of the last run, on the final
    build, with nothing else loading the machine and no log listener. The mod
    was installed in place and then switched off and on four times. After
    every load all six were in tray 2, and after every unload all six were
    back in Explorer.

**Not verified:**

* A refused version and Explorer re-entering the subclass have not been seen
  live. A refused add has, but only for Explorer's own icons.
* Whether asking about refused adds inside the burst is what cost Telemachus
  its icon, or the log listener and the load alone. The last run had neither,
  and lost nothing in five loads.
* Both suites leave the XAML half out.

## 2026-09-23 - Fixes from a second external review

**Impact:** fixes: moving icons into and out of Explorer's tray, icon handles
in the embedded tray, and the order the mod starts and stops in.

The same reviewer went over the previous entry's fixes with a harness of
functions extracted from the source. It confirmed seven of them and reproduced
four remaining defects. Each was checked against the code here and is now a
regression test; the harness itself was not run, since it builds on Linux with
Windows stubbed out. The reviewer counted the balloon and renumbering
limitations as settled by the README, and agreed the replay message no longer
trusts what it carries.

* **Moves are settled from the icon store, not queued as records**
  (DECISIONS 66). Three of the findings were one design fault:
  * **A move carried stale data.** The add was built when the move was asked
    for. A new callback or version sent before the taskbar's thread got to it
    was swallowed, since Explorer did not have the icon yet, and missing from
    the add.
  * **Two moves could be delivered in the wrong order.** Each was prepared
    under the store's lock and queued after it was released, so an older
    removal could follow a newer add: the icon was gone from Explorer while
    the mod believed it was there. A removal was always delivered, whatever
    had happened since.
  * **A refused add was recorded as taken.** Nothing asked again, and the icon
    was in neither tray.

  A move now changes only where the icon should be, and wakes the taskbar's
  thread. That thread compares it with where Explorer has the icon, and hands
  Explorer an add built from the icon as it is then, with its version, or a
  delete (`SettleShellIcons`). There is no order to get wrong and nothing to
  go stale. Explorer's answer is what is recorded:
  * A refused add is followed by a modify, which Explorer takes only for an
    icon it has already.
  * A real refusal is asked again on the tray thread's next ticks, three times
    in all, and the icon is then left to its application (`OwedToShell`). Its
    own adds and modifies go to Explorer, and Explorer's answers are recorded.
    Explorer answers a modify for an icon it does not have with failure, and an
    application that recovers adds its icon again. Retrying for ever would send
    Explorer the record it refused.
  * An icon its application removed while Explorer was taking it back is taken
    out again.
* **The embedded tray drew from icon handles it did not own** (DECISIONS 60).
  The previous fix gave the floating trays and the arrange window their own
  copies. The tray in the taskbar, and its overflow popup, which draws what the
  last refresh kept whenever it is opened, still held the store's handles. The
  tray thread's watchdog destroys those when an icon's application goes.
  `CellSnapshotsOf` now copies them under the lock for both.
* **The start and stop order** (DECISIONS 67):
  * `Wh_ModInit` attached to Explorer and then started the tray thread. A
    thread that could not start, or could not register its window classes,
    left the mod swallowing icons into trays nothing drew. A failed
    `CreateThread` returned failure with the subclass still attached, to a
    module Windhawk was about to unload. The thread now starts first.
    `Wh_ModInit` waits for it, a few milliseconds, and fails with nothing
    attached if it gave up. The window classes' registration results are
    checked.
  * The tray thread was stopped in `Wh_ModUninit`, after Windhawk has removed
    the mod's hooks, and that thread installs hooks of its own. It is now
    stopped in `Wh_ModBeforeUninit`, and does not apply hooks once unloading
    has begun.
  * Pinning the module when the thread will not stop is kept as containment.
    The log now says whether the pin took, and that the mod then stays loaded
    until Explorer exits.
* The integration test's stand-in for Explorer now answers as Explorer does:
  it refuses an add for an icon it has, and anything else for one it does not
  have. It used to take everything, so it could not tell the mod apart from one
  that records a refusal as success (DECISIONS 21).
* `tools/install.ps1 -Enable` on its own failed with "Parameter set cannot be
  resolved": `-Enable` belongs to both the 'Install' and the 'Enable' sets.
  `-Install` is now mandatory in its own set. Found switching the mod off and
  on for the live check below.

**Verified:**

* Regression 757/0. New tests:
  * `Test_AnAddCarriesWhatArrivedWhileItWaited`: the callback, tooltip and
    version sent while the move waited are in the add
  * `Test_WhatExplorerIsHandedFollowsTheLastMove`
  * `Test_AnAddExplorerRefusesIsAskedAgainThenLeftToItsApplication`
  * `Test_AnAddRefusedForAnIconExplorerHasIsRecordedAsThere`
  * `Test_AnIconRemovedWhileExplorerTookItBackIsTakenOutAgain`: the removal
    is fed from inside the stand-in's answer, as Explorer's own sent messages
    would come back
  * `Test_AnEmbeddedTrayDrawsFromItsOwnCopyOfEachIcon`

  Two queue tests now say what the store does in their place: an icon removed
  before its move is not added back, and a move undone before the taskbar's
  thread gets to it hands Explorer nothing.
* Integration 200/0. New phases and checks:
  * [11c]: an update sent while a move waits is in the add Explorer gets.
  * [11d]: a stand-in that refuses every add is asked three times and no more.
    The application's update then fails, its re-add is taken, and moving the
    icon away takes it out of the stand-in.
  * [1]: the tray thread is running when `Wh_ModInit` returns.
  * [16]: it has stopped when `Wh_ModBeforeUninit` returns. The phase now
    sets `Wh_ModInit`'s wait to 0 (`g_trayThreadStartWaitMs`), so the unload
    still comes before the thread's window exists, as it does after a start
    slower than the wait. With the wait in place the phase could no longer
    reach that case, and the first mutation run showed it: the mutant that
    sends the shutdown only once survived, 69/70.
* Eleven new mutants, in `tools/mutants_reviews.py` with the other review
  mutants, so no module passes 400 lines. The two that described the queue are
  replaced by one for the store. Mutation score 70/70 = 100%.
* The mod compiles clean with the Windhawk editor's flags, XAML half included.
  Windhawk's pull request validation reports no warnings.
* Live, in Explorer:
  * **After a restart** (`redeploy.ps1`): the tray thread was running before the
    mod attached, and tray 2 went into the second taskbar with its six icons,
    five on the bar and one in its overflow, drawn from `CellSnapshotsOf`.
  * **A move and back**, through the arrange window with posted keys and no
    pointers (DECISIONS 42). Asked from outside where Telemachus was
    (`Shell_NotifyIconGetRect`): the mod said tray 2 (-208,1146); after the move,
    Explorer said the primary taskbar (1653,1037), so it had taken the add; after
    the move back, the mod said tray 2 again. No refusal was logged.
  * **Switched off in place:** unloaded in 0.23 seconds, with no "did not stop".
    Explorer then answered for Telemachus itself: it had been put back.
  * **Switched on in place:** loaded into the running Explorer, asked the
    applications to register again, took tray 2's six icons back from Explorer,
    and attached the tray from the tree walk. Screenshots of both trays and of
    the main tray's overflow: none of tray 2's icons was left in the main tray.

**Not verified:**

* Tray 2's overflow popup was not opened live; its icons come from the same
  snapshots as the bar's.
* The check for unloading before hooks are applied: the hooks were in place
  long before the mod was switched off. Both suites leave the XAML half out.
* No live move was refused, so the refusal path has run only in the tests.
* No test makes the tray thread fail to start, so `Wh_ModInit` failing with
  nothing attached is untested.

## 2026-09-23 - Loading into a running Explorer

**Impact:** fixes: what a user sees on installing, updating or switching the
mod on, all of which load it into an Explorer that is already running.

Found live while checking the review fixes: Split Tray was switched off and on
in Windhawk. Until now every live run had restarted Explorer (`redeploy.ps1`),
which is not how users meet the mod.

* **The tray went into the wrong place after a reload** (DECISIONS 65). With
  the icons already built, only the tree walk can find the taskbar's tray, and
  it anchored on `SystemTrayFrame`: first in the tree, and above the row the
  tray goes into. So `FindTrayRow` found no row and settled for the Grid beside
  the clock. The code's comment said candidates were ordered icons first; they
  were not. `AnchorPreference` now orders them, and the walk anchors on an
  `IconView`, as the hook does on a fresh start.
* **Icons were left in the main tray as well** (DECISIONS 64). Unloading puts
  every icon back into Explorer; loading again swallowed the re-registrations
  of icons going to tray 2, and Explorer kept its copies. An icon new to the
  mod that goes to one of its trays is now taken back from Explorer with a
  `NIM_DELETE`. The log line added in the previous entry, "Explorer did not take
  back icon", showed it: the next unload's add was refused because Explorer
  still had the icon.
* **SystemInformer's icons stayed in the main tray** (DECISIONS 63). They are
  only ever updated while Explorer has them, and an update carries no path, so
  the rule sending SystemInformer to tray 2 could not match them. When a
  message carries no path and the icon's tray depends on one, the mod now asks
  Windows which program owns its window, once per icon. That refines
  DECISIONS 4, with the user's agreement.
* **...and then could not be clicked.** Placed from an update, they were
  accepted, so SystemInformer never added them again. They had no callback, so
  clicks did nothing, and Explorer refused them back when the mod unloaded. An
  update for an icon the mod has never seen added is now answered with
  failure, as Explorer would answer. SystemInformer adds the icon again, whole.
* `docs/development.md` now says to check a reload in place, not only a
  restart.

**Verified:**

* Regression 712/0. New tests:
  * `Test_AnIconThatOnlyUpdatesIsPlacedByItsProgram`: the lookup is asked
    once, and only when no path came
  * `Test_OnlyAnIconNewToTheModIsTakenBackFromExplorer`
  * `Test_TheTreeWalkAnchorsOnAnIconBeforeTheFrame`

  The tests' lookup finds nothing unless a test says otherwise, because their
  made-up window handles could be real windows.
* Integration 185/0. New phase [13b]: the mod loads into a stand-in shell that
  already holds two icons:
  * One is registered again, and is taken back.
  * The other only updates. Its update fails, its application adds it again,
    and the icon ends up in tray 2 with its callback and gone from the shell.
* Four new mutants. Mutation score 60/60 = 100%.
* Live, after the fixes, over two reloads in place:
  * The tree walk anchored on `SystemTray.IconView`, and tray 2 went into
    `SystemTrayFrameGrid` with all six icons. Before the fixes it went into the
    Grid, with two.
  * SystemInformer added its four icons in full after its first update was
    refused, both in a reload in place and after a restart.
  * The user confirmed that SystemInformer answered a click in tray 2, and that
    nothing was left in the main tray after switching the mod off and on. The
    log of that last switch was not captured: the listener came back empty.

## 2026-09-23 - Fixes from an external review

**Impact:** fixes: unloading and reloading, resource ownership, replay races,
protocol gaps, and two defects in how an icon is tracked.

An external review of the source, with a small harness of its own, listed
lifecycle, ownership and protocol defects. Each finding was checked against
the code here and reproduced by a new test before it was fixed. The harness
itself was not run; the reproductions are in this project's own suites, which
compile the shipped source. Also found while preparing the Windhawk
submission: its pull request validation asks each symbol hook array to name
its DLL, now `taskbarDllHooks` and `systemTrayDllHooks`, and it reports no
warnings for the mod.

* **The arrange window's class outlived the mod** (DECISIONS 59). It was
  registered against Explorer's module and never unregistered, so after the mod
  was reloaded its next arrange window would have run the unloaded window
  procedure. All three classes now belong to the mod's module
  (`ModuleInstance`), are registered together by the tray thread, and are
  unregistered as it ends. A class left behind by an earlier build is cleared
  first.
* **The arrange window freed its image list several times.** One image list
  was shared by every list without `LVS_SHAREIMAGELISTS`, so each list
  destroyed it, after the window had already done so. The lists now share it,
  and the window destroys it once, in `WM_NCDESTROY`. The window's font, made
  once, is released when the thread ends.
* **Unloading could finish with the tray thread still running** (DECISIONS 59).
  The shutdown was posted only if the thread's window already existed, so
  unloading straight after loading lost it, waited four seconds and went on.
  `StopTrayThread` now posts it as soon as there is a window. If the thread
  still does not stop, the module is pinned: a leak rather than a crash.
* **A queued move could recreate an icon its application had removed**
  (DECISIONS 58). Applications send their messages; replays were posted. So an
  application's `NIM_DELETE` was handled before an earlier move into the
  primary tray, and delivering the move afterwards put the icon back. Its
  delete had also gone to Explorer, because the move was recorded as done when
  it was queued. Now:
  * Replays wait in a queue inside the mod, and the registered message only
    wakes the taskbar's thread. It used to carry a pointer that the subclass
    delivered and deleted, and any process on the desktop can post it.
  * A move takes effect when it is delivered: `shellTarget` when queued,
    `forwardedToShell` when Explorer is handed it.
  * Each move is a generation. An add whose icon has gone, or has been moved
    again, is dropped. Unloading delivers the queue before it restores icons.
* **Floating trays and the arrange window drew icons that could be gone**
  (DECISIONS 60). They copied bare handles out under the lock and drew them
  after releasing it, while the taskbar's thread destroyed the old picture on
  every icon change. They now draw from their own copies (`OwnedIcon`).
* **An unknown taskbar layout was guessed at** (DECISIONS 61). An unrecognised
  `TaskbarHost::FrameHeight` fell back to offset 0x10 and dereferenced what it
  found. `ElementOffsetFromFrameHeight` answers "no" instead, and the tray
  floats.
* **A record of another shape was read at the captured offsets**
  (DECISIONS 57). It is now left to Explorer untouched, and logged once.
* **A tooltip update brought back an icon its application had hidden.**
  Whether an icon is hidden was read from the message alone, not from its
  state once the message was applied.
* **An icon re-registered by GUID kept its old owner.** An application that
  restarts registers its GUID from a new window. The folded record took the
  new owner, but the icon did not, so clicks went to the old window and the
  watchdog, which checks the old window, dropped the icon.
* **Version 3 and 4 icons were not clicked the way Explorer clicks them**
  (DECISIONS 62). There was no `NIN_SELECT` after a left button-up, and no
  `WM_CONTEXTMENU` for version 3. Applications written to the newer protocol,
  Microsoft's NotificationIcon sample among them, act on those. The sequence
  is now `TrayCallbacksFor`.

**Not changed, documented instead:**

* **Unplugging a display renumbers every tray after it.** That includes the
  extra trays, so their icons show in whichever tray has their number now.
  That is DECISIONS 53. The README's feature list promised more than that, and
  now points to Known limits, which spells it out.
* **Balloons.** A balloon for an icon that lives only in Split Tray's trays is
  not shown, since Windows shows balloons only for icons it has. "Balloon
  notifications stay with the main tray" said otherwise. The README and the
  mod's readme now say so, and suggest "both" for applications whose
  notifications matter.

**Not changed:**

* **Rebuilding the XAML bitmaps.** Every shown icon's bitmap is rebuilt when
  the layout is unchanged; per-icon dirty tracking can come later.
* **Beginning shutdown in `Wh_ModBeforeUninit`.** It was suggested; the
  unload order above does not need it.

**Verified:**

* Regression 693/0. New tests:
  * `Test_ARecordOfAnotherShapeIsLeftToExplorer`, which replaces the truncated
    payload test and asserts the opposite, per DECISIONS 57
  * `Test_ATooltipUpdateDoesNotRevealAnIconItsApplicationHid`
  * `Test_AnIconReRegisteredByGuidIsClickedInItsNewWindow`
  * `Test_AFloatingTrayDrawsFromItsOwnCopyOfEachIcon`
  * `Test_AMoveTakesEffectWhenExplorerIsHandedIt`
  * `Test_AQueuedAddForARemovedIconIsDropped`
  * `Test_AnOvertakenMoveIsDroppedAndARemovalAlwaysDelivered`
  * `Test_TrayCallbacksAreWhatExplorerSends`
  * `Test_OnlyAKnownTaskbarHostLayoutIsUsed`

  Tests that read `forwardedToShell` after a move now deliver the queue first,
  as the taskbar's thread would.
* Integration 169/0. New phases:
  * [8b] a version 4 click, in full
  * [9b] the application's delete overtaking a queued move
  * [11b] the arrange window's lists sharing the image list, closed and opened
    again
  * [12] no class left registered after unload
  * [16] unloading straight after loading
  * [17] a replay message posted with a bogus lParam, which crashed the test
    process before the fix
* Before the fixes, every new test failed, and [17] crashed.
* Twelve new mutants. Mutation score 56/56 = 100%. The catalogue moved to
  `tools/mutants.py`, which keeps `tools/mutate.py` under the module size
  limit.
* MSP fitness 85.7/100 PASS. It measures `tools/` only; testability is 0.
* Windhawk's `pr_validation.py`, run locally on the mod file: no warnings.
* Not tested:
  * the module pin when the tray thread does not stop
  * the timer waking replay delivery again after a lost wake-up
  * the arrange window's rows drawing from their own copies
  * the unrecognised-layout path in a live Explorer (the decoding is unit
    tested)

**Live, in Explorer:**

* Tray 2 embedded as before. The offset decoding recognised this build's
  `TaskbarHost::FrameHeight`: the log says "XamlRoot resolved".
* The arrange window opened, closed and opened again. Both of its lists had
  `LVS_SHAREIMAGELISTS`, checked from outside the process.
* After Windhawk switched the mod off and on in the same Explorer, the arrange
  window opened again, and Explorer carried on.
* Telemachus in tray 2 answered left and right clicks. Its icon is at version
  0, so `NIN_SELECT` was not exercised live.
* The first reload in place showed three older defects, fixed in the next
  entry.

## 2026-09-23 - An extra tray can be disabled

**Impact:** a feature (a new per-tray setting).

Asked for: the floating third tray on the primary display worked, and the user
wants to keep it for testing but have it off for now.

* **The switch.** Each entry in Extra trays has a **Disabled** switch
  (`extraTrays[n].disabled`). A disabled tray is not drawn, not even as the
  empty-tray handle, and is left out of the menus. Icons meant for it wait in
  tray 1, and they go back to it when it is enabled again, through the same
  path as a display being plugged back in (`PlanTrays` marks it unavailable).
* **It keeps its number.** A disabled tray keeps its number, as an extra tray
  on a missing display does, so the trays after it do not shift (DECISIONS 56).
* **Off, not on.** The switch is off by default, so that a value that is not
  there reads as on. Windhawk reads a missing value as 0. A switch called
  "enabled" would have hidden every tray saved before it existed, with nothing
  in the log to say so (the same trap as `embedInTaskbar`, see
  `tools/install.ps1`).
* **Header.** The mod's `@github` now points at the publishing account, with
  the repository as `@homepage`.

**Verified:**

* Regression 651/0. New tests:
  `Test_ADisabledExtraTrayKeepsItsNumberButIsNotThere` (the anchor) and
  `Test_ADisabledTrayIsNotDrawnAndItsIconsWaitInThePrimaryTray`. The
  settings-reading test also checks that a missing switch reads as on.
* Integration 134/0. Phase [8] disables the third tray while an icon is in it:
  the tray's window goes, the icon goes back to the shell, and the tray keeps
  its number. Enabling it again brings the icon back.
* Three new mutants (a disabled tray still shown, the switch read inverted, and
  a disabled tray described as present), plus the numbering mutant repointed.
  Mutation score 44/44 = 100%.

## 2026-09-23 - A tray on every display, and ready to publish

**Impact:** a feature, a fix, and the repository prepared for release.

Asked for: support for any number of additional displays rather than exactly
one, tested by adding a third tray elsewhere on display 1, since this machine
has two displays. Also: brand the tool as Split Tray, update the README and
docs, and prepare the repository for public release.

**Trays are numbered.** Tray 1 is Explorer's own. Split Tray's are 2 and up:
one for every display other than the primary one, left to right, then any
extra trays from the settings (`PlanTrays`, DECISIONS 53).

* **Destinations name a tray.** A destination is now a tray number, with an
  optional "and the primary tray". Rules offer trays 1 to 6 and "both".
* **Old names keep their meaning.** `secondary` is tray 2, and stored
  placements `p` and `s` still read as trays 1 and 2 (`ParsePlacementCode`).
  Later trays are stored by number.
* **Settings.** The `secondaryMonitor` setting is gone; every display gets a
  tray. A new `extraTrays` list adds floating trays on any display: `primary`,
  or a display number counted from the left, and a corner.
* **Missing trays.** An icon whose tray is missing waits in the primary tray and
  returns when the tray does, now per tray. `ReconcileIconLocked` puts each icon
  where its destination says, given which trays exist. It is shared by settings
  changes, display changes and moves.
* **A latent bug fixed along the way.** With "Show icons the application marked
  as hidden" turned off, an icon its application hid (`NIS_HIDDEN`) is kept out
  of Split Tray's trays when it arrives. The old settings replay ignored that,
  so the first settings change put the icon in the secondary tray anyway.
  `ReconcileIconLocked` checks it, and the store now keeps an icon's state even
  while it is only in the primary tray, so the check has something to go on.

**Each tray draws itself.**

* **In the taskbars.** On the taskbar's thread, a list of `EmbeddedTray`s (one
  per display) replaces the single set of panel globals. Each has its own
  taskbar window, root, panel, cell size, overflow and drawn-state, so a
  display at a different scale gets cells its own size.
* **Floating.** The tray thread has a hidden controller window for messages,
  the timer and Explorer's `TaskbarCreated` broadcast, plus one floating panel
  per tray that is not embedded. An empty floating tray draws a handle. Every
  floating tray has the same Shift+right-click menu as an embedded one.
* **Menus and the arrange window.** Both offer every tray. The arrange window
  has one list per tray; the number keys send an icon to that tray, and H
  moves it in or out of its tray's overflow.
* **Unloading** now takes the panels out on the taskbar's own thread. Doing it
  from Windhawk's unload thread threw inside a catch-all and left them in
  place.

**A fix found on the way: clicks could reach the wrong application.** An
embedded cell carried its position in the drawn order. A click looked the icon
up at that position in the store, which is in registration order, not the
user's order. After icons had been reordered, or moved out and back, the two
differed. Every icon now has a serial number for its lifetime, and cells carry
that (DECISIONS 54). The same serial answers `Shell_NotifyIconGetRect`.

**Test seam.** `RecomputeGeometryLocked` reads the displays through
`g_enumerateMonitors`. The regression tests supply a fixed set, so what they
check no longer depends on the machine that runs them.

**Live, 04:19 (Explorer restarted, with one extra tray on the primary display):**

* The mod logged tray 2 as the display to the left and tray 3 as floating on
  the primary display.
* Tray 3's window, `SplitTrayFloatingTray`, is at (8,996)-(36,1024): the
  bottom-left corner of the primary work area. It shows its handle.
* Tray 2's floating fallback was replaced by the embedded tray once it
  attached. UI Automation shows its six icons in the second taskbar.
* Asking where Telemachus's icon is returned (-178,1146)-(-138,1194), centred
  on its image, so the serial lookup answers correctly live.
* The mutation check was compiling in the background, and it slowed Explorer's
  start and the log capture: 17 lines in 50 seconds.
* Not seen live: an icon moved into tray 3 and clicked there. The integration
  test covers that path with real shell32; the live move is left for the user.

**Release.**

* **Branding.** The in-Windhawk readme, the description and every settings
  label say Split Tray and describe numbered trays.
* **New files.** `README.md` is rewritten for someone meeting the project for
  the first time. New `docs/architecture.md`, `docs/development.md`,
  `CONTRIBUTING.md`, and an MIT `LICENSE`. `docs/xaml-injection-plan.md` is
  marked as the design record it now is.
* **The public branch.** `main` is built as a single commit of the curated tree
  (DECISIONS 55).

**Verification**

* Regression 635 / 0 (was 501). Two sets of tests were replaced:
  * The three monitor-selection tests went with `SelectSecondaryMonitor`. They
    are replaced by tests of `PlanTrays` and `ResolveDisplay`.
  * The arrange window's default-move test now describes one list per tray.
  * The store tests now run on displays the test supplies, not the machine's.
* Integration 126 / 0 (was 99). The test gives itself two extra trays on the
  primary display, so it runs on a machine with one display instead of skipping.
  New phase [8]: a third tray at the top left of the primary display. An icon
  moves into it without Explorer being told, a click on it reaches the owner, and
  the icon's location answers with that tray's cell. Phases after it are
  renumbered.
* Mutation `tools/mutate.py`: 41 / 41 killed (100%). The two routing mutants
  now target `PlanFor`'s tray form. There are twelve new ones:
  * a tray for the primary display
  * display trays marked as not connected
  * extras giving up their number
  * the last display unreachable by number
  * tray 2 stored in a form older versions cannot read
  * every icon shown in tray 2
  * a hidden icon re-sorted into a tray
  * a floating tray ignoring its corner
  * an empty floating tray with nothing to click
  * a serial lookup that returns the first icon
* Compile check clean, hygiene clean, settings lint 20/20, symbols 5/5.
* MSP fitness 80.0 / 100, pass. It measures `tools/` only, as before.

**Regression anchors**

* `Test_DestinationsNameTrays`, `Test_PlacementCodesReadWhatEveryVersionWrote`,
  `Test_EveryDisplayButThePrimaryGetsATray`,
  `Test_OneDisplayMeansNoTraysUnlessAsked`,
  `Test_ExtraTraysComeAfterTheDisplaysAndKeepTheirNumbers`,
  `Test_DisplaysAreFoundByNumberOrAsPrimary`,
  `Test_TraysAreDescribedByWhereTheyAre`, `Test_ARuleCanSendAnIconToAnyTray`,
  `Test_AnEmptyFloatingTrayStillHasAHandle`,
  `Test_MovingBetweenSplitTraysTraysLeavesTheShellAlone`,
  `Test_AnIconWhoseTrayGoesAwayFallsBackAndReturns`,
  `Test_AnIconForATrayThatIsNotThereWaitsInThePrimaryTray`,
  `Test_AnIconItsApplicationHidStaysHiddenAfterASettingsChange`,
  `Test_EveryIconHasItsOwnSerial`; integration [8].

---

## 2026-09-23 - Clicks on a Tauri application's icon in the secondary tray

**Impact:** a fix.

Reported: once Telemachus's icon is in the secondary tray it stops responding -
no menu, and clicking it does not bring the window back - until it is moved
back to the primary tray.

**Tauri's tray library asks where its icon is before it handles a click.**
Telemachus is a Tauri application (its window is
`dev.telemachus.controller-siw`). On Windows its tray library (`tray-icon`
0.24, `src/platform_impl/windows/mod.rs`) calls `Shell_NotifyIconGetRect` for
every mouse message it gets, and returns without doing anything when that fails:
no click event, no menu. The mod forwarded the click correctly. The application
then asked Explorer where the icon was, and Explorer, which does not have an
icon that lives only in the secondary tray, said it had none. Desk Tray uses the
same library, so its icons would have done the same in the secondary tray.

**What the question looks like.** The wire probe now asks it too
(`RunRectQueries`; evidence in `tests/probe/probe-rect-output-26100.txt`).
shell32 sends two `WM_COPYDATA` messages to `Shell_TrayWnd` with `dwData = 3`
and a 40-byte record: the tray signature, the part asked for (2 for the size,
sent first, then 1 for the position), the owner window at 0x10, the uID at 0x14
and the GUID at 0x18. It builds the rect from the two answers, each packed like
a mouse position with signed halves. A size of 0 means there is no such icon,
and the caller gets `E_FAIL`; a position of 0 is accepted.

**The fix.** The subclass answers the question for icons that live only in the
secondary tray (`SecondaryOnlyIconIndexLocked`), with where the mod drew them:
the cell in the embedded tray; the chevron, for an icon in the overflow, since a
click there closes the popup before the application gets to ask; or the cell in
the floating window when the embedded tray is not up (`CellScreenRect`).
Everything else - icons in the primary tray, icons shown in both, icons nobody
has - still goes to Explorer. XAML gives a cell's bounds in device-independent
pixels from the island's corner, and the island fills the taskbar window
(measured: its content bridge has exactly the window's rect on both taskbars).
So the bounds are scaled by the root's rasterization scale and offset by the
window's corner (`IslandBoundsToScreen`). The first answer for each icon is
logged and later ones are not: applications ask on every click, and this runs
on the taskbar's thread (DECISIONS 51).

Reproduced first in the integration test with real shell32. Its stand-in shell
now answers the question as Explorer does, for the icons it holds, and asking
about an icon in the secondary tray returned `E_FAIL` - what Telemachus got.
After the fix it returns the icon's cell in the mod's window.

**Live**

* Before deploying, with Telemachus in the secondary tray, asking
  `Shell_NotifyIconGetRect` for its icon (window `0x631586`, uID 2) returned
  `0x80004005`. Desk Tray's icons in the primary tray answered normally.
* Deployed 00:14: the same question returned `S_OK`, (-208,1146)-(-168,1194),
  and the mod logged `told telemachus.exe where its icon is` with that rect.
  UI Automation, read per-monitor DPI aware, puts the sixth image in the
  secondary tray - Telemachus's, after SystemInformer's four and the usage
  monitor - at (-198,1160)-(-178,1180), centred in that cell, with the clock
  button beside it spanning the same 1146-1194. The taskbar window is
  (-1920,1140)-(0,1200) at 125%.
* All three Desk Tray icons registered once, to the primary tray, after
  Explorer's announcement at 31.8 s.
* Not seen here: a real click on Telemachus in the secondary tray. That is left
  for the user.

A measurement pitfall worth keeping: PowerShell's process already has a DPI
awareness, so `SetProcessDpiAwarenessContext` fails quietly there and every
coordinate on the 125% monitor comes back scaled down. The first measurement of
the secondary taskbar read (-1920,912)-(-384,960) for that reason.
`SetThreadDpiAwarenessContext` works.

**Verification**

* Regression 501 / 0 (was 425). Integration 99 / 0 (was 85): new phase [7] asks
  about an icon in the secondary tray, one in the primary tray and one nobody
  has. The phases after it are renumbered.
* Mutation `tools/mutate.py`: 31 / 31 killed (100%). Eight new mutants: nobody
  answering, answering for Explorer's icons, size and position swapped, an
  empty rect reported as found, the wrong owner offset, the GUID ignored, a
  cell placed by row, the display scale ignored.
* Compile check clean, hygiene clean, settings lint, symbols 5/5.
* MSP fitness 80.0 / 100, pass - `tools/` only, as before; `cycles` and
  `fan_out` n/a, `testability` fails.

**Regression anchors**

* `Test_ParsesAnIconRectQuery`, `Test_RejectsWhatIsNotAnIconRectQuery`,
  `Test_IconRectReplyIsWhatShell32Reads`,
  `Test_CellScreenRectIsWhereTheCellIsPainted`,
  `Test_IslandBoundsScaleToTheScreen`,
  `Test_TheModAnswersWhereOnlyForIconsTheShellDoesNotHave`,
  `Test_AnIconAskedAboutByGuidIsFoundByGuid`; integration [7].

---

## 2026-09-22 - Icons that came back empty, icons registered twice, and a stalled taskbar

**Impact:** three fixes.

Reported: the Claude usage monitor's icon vanished after being moved to the
secondary tray and back, and Desk Tray has three icons of which one or two
sometimes go missing.

**An icon moved back came back as its last message.** Putting an icon into the
primary tray - moving it back, "Reset moved icons", a settings change, unloading
the mod - replays a stored record into the shell as `NIM_ADD`. That record was
simply the last message seen, and the last message is usually a partial modify:
the usage monitor changes its picture and its tooltip in separate `NIM_MODIFY`s,
and destroys each picture it replaces. Reproduced with real shell32 in the
integration test before anything was changed: the re-add arrived as
`flags 0x2` - picture only - with no callback message, no tooltip, no
executable path (a modify never carries one) and a picture handle already
destroyed. An icon with nothing to draw and nothing to click.

Every message is now folded into one record (`FoldTrayRecord`), field by field
as the shell applies a partial modify: an add starts it afresh, a modify updates
only what its flags name, `NIS_*` state goes through its mask, a modify's empty
path never overwrites the owner's, and a balloon (`NIF_INFO`) is not kept, so a
replay cannot show it again. The add is drawn with a fresh copy of the mod's own
picture, owned by the request until the shell has taken its copy
(`AddRecordFor`), and followed by the `NIM_SETVERSION` the application
negotiated, since a re-added icon starts again at version 0. After the fix the
same test re-adds `flags 0x7`, the right callback and tooltip, a live picture
and the path.

`NIM_SETVERSION` for an icon the shell was never given is no longer forwarded:
the shell fails the call, and the application is told its tray does not support
the version it asked for. It is recorded and replayed with the add instead.
`Test_SetVersionIsRecordedAndForwarded` asserted the old behaviour and is now
`Test_SetVersionIsRecordedAndOnlyForwardedToAShellThatHasTheIcon`.

**Every application registered twice on an Explorer start.** The mod asked for
every icon (`TaskbarCreated`) as soon as it attached - about 3 seconds in -
while Explorer announces its own taskbar, and every application re-registers,
once its tray is ready, at about 17. The live log showed Desk Tray's
"WhatsApp (default)" answering the mod and not Explorer; it was forwarded to
the primary tray, Windows has it marked as promoted, and UI Automation showed it
was not in the tray at all. The one re-register request sent by hand earlier
today brought it straight back. Registered into a tray that was not ready, it
was dropped.

The logic was also the wrong way round. On a running Explorer, `Wh_ModInit`
attached straight away, and the later check saw the attachment and returned
without asking - the one case that needs it. The mod now asks only when Explorer
will not (`ShouldAskAppsToReRegister`): when it loaded into a taskbar that
already existed, or when Explorer's announcement went out before the mod was
watching - which its own tray window now hears, and logs.

**A stalled taskbar lost icons as well.** Deployed with only the two fixes
above, Explorer's single announcement still left two of Desk Tray's three icons
out of the tray, although Desk Tray had all three `tray_icon_app` windows alive.
Desk Tray is a Tauri app, and its tray library re-adds an icon straight after
deleting it and never retries a failed add. The deletes arrived; the adds landed
in the 4-5 seconds the mod's XAML tree dump held the taskbar's thread -
46.2-50.1 s in, and 23.4-28.5 s in the earlier capture - and shell32 gives up
on a tray that does not answer. `dumpXamlTree` is off by default and was on only
because earlier diagnostic runs had turned it on; it is off again, and its
description now says what it costs. With it off, all three icons registered and
UI Automation found all three in the tray.

For the same reason a swallowed `NIM_MODIFY` is no longer logged. SystemInformer
sends four a second, the handler runs on the taskbar's thread, and with a log
viewer attached each line costs tens of milliseconds there (DECISIONS 44).

The test harness's `Wh_Log` now takes a lock. The real one is safe to call from
any thread and the mod does, and the integration test reads the log while the
tray thread writes it (DECISIONS 21).

**Live**

* 23:36, fold and re-register fixes only: `not asking applications to
  re-register` at 4 s, `Explorer announced its taskbar` at 39.8 s, heard by the
  mod's own window while it was watching, and each application registered once.
  Desk Tray lost two icons to the dump, as above.
* 23:43, dump off: all three Desk Tray icons deleted and re-added within
  1.3 s of each other, all forwarded to the primary tray; UI Automation lists
  "WhatsApp (primary)", "WhatsApp (default)" and "Facebook (default)" in it.
* Not seen live: an icon moved away and back. The usage monitor was not running
  at the time, so that rests on the integration test with real shell32.

**Verification**

* Regression 425 / 0 (was 376). Integration 85 / 0 (was 53): new phases [9]
  (moved away and back, with the usage monitor's exact update pattern) and [13]
  (a taskbar announced before the mod was watching it); [1] and [12] now check
  the re-register decision for a running Explorer and an Explorer start.
* Mutation `tools/mutate.py`: 23 / 23 killed (100%). Ten new mutants cover the
  fold, the picture, the path, the balloon, the version and the re-register
  decision. Three older mutants had stopped matching the source and were being
  reported as survivors; their patterns now follow the code, and all three are
  killed.
* Compile check clean, hygiene clean, settings lint, symbols 5/5.
* MSP fitness 80.0 / 100, pass. It measures `tools/` only; `cycles` and
  `fan_out` read n/a, and `testability` fails because the tools have no
  matching test files.

**Regression anchors**

* `Test_FoldKeepsWhatAPartialModifyLeavesOut`, `Test_FoldDoesNotKeepABalloon`,
  `Test_FoldAppliesStateThroughItsMask`, `Test_AnAddStartsTheRecordAfresh`,
  `Test_AddRecordDrawsWithTheModsOwnPicture`,
  `Test_SetVersionRecordCarriesTheVersion`,
  `Test_TheStoreKeepsTheWholeIconNotTheLastMessage`,
  `Test_PuttingAnIconBackReplaysItsVersionToo`,
  `Test_OnlyAsksAppsToReRegisterWhenExplorerWillNot`; integration [9] and [13].

---

## 2026-09-22 - The overflow popup, drawn in its own window

**Impact:** a fix.

Reported with a screenshot: the overflow popup was a wide grey box with the icon
pushed against its bottom edge and cut off.

A `Flyout` is drawn inside the XAML root it belongs to unless told otherwise, and
this one belongs to the taskbar - an island about 48 DIP tall. The popup was being
squeezed into that strip. The mod's own `MenuFlyout` never showed the problem
because menus default to a window of their own. `ShouldConstrainToRootBounds(false)`
gives the popup one too.

The rest is styling it for what it holds: the presenter's defaults are for a
flyout with prose in it - a minimum width, generous padding, square corners. It
now has 4 DIP of padding, no minimum size and 8 DIP corners, and the icons sit in
a grid of 40 DIP square cells, at most five to a row, rather than a strip of the
taskbar's tall, narrow cells. Clicking an icon in it closes it, as the native one
does, so it does not sit over the window or menu the application opens. Hover
and press washes on every cell, and the chevron, are rounded like the native
tray's.

**Verification**

* Regression 376 / 0, integration 53 / 0, compile check clean, hygiene clean,
  settings lint 19/19, symbols 5/5. `winrt/Windows.UI.Xaml.Interop.h` added for
  `xaml_typename`; the first compile caught its absence.
* Not verified on screen yet.

---

## 2026-09-22 - What the drag log showed, and a reset that missed a name

**Impact:** a tooling fix, and evidence.

**The drag evidence**

A capture from a redeploy run by hand, on the build before the in-place refresh,
caught two drags and a click on the secondary tray:

```
21:00:17.078  PointerPressed on a cell (draggable=1)
21:00:17.158  PointerMoved on a cell (pointerDown=1)
21:00:17.194  started from slot 0
21:00:17.643  icon order updated (4 entries)
21:00:17.907  PointerPressed ... 21:00:18.000 started from slot 0
21:00:18.546  icon order updated (4 entries)
21:00:38.692  PointerPressed -> forwarded 0x201 / 0x202 to SystemInformer uID 2
```

So the press arrives, moves follow, the threshold is crossed, and the drag runs
to its release and saves - after the `ShiftCellTo` fix, the in-island drag works
mechanically. Clicks are forwarded to the owning application. The saved order
was 2, 3, 5, 14, which is the order the icons registered in, so both drags ended
where they began; the log cannot say whether the icon visibly moved under the
pointer on the way.

**The reset missed a name**

`redeploy.ps1 -ClearPlacements` removed a value called `iconOrder`. The mod calls
it `embeddedIconOrder`, so the order was never cleared - and a
`Remove-ItemProperty -ErrorAction SilentlyContinue` on a name that does not exist
says nothing. The script now reads the storage value names out of the mod source
(`constexpr PCWSTR k...Value = L"..."`) instead of keeping its own copy, and
refuses to run a clear if it finds none. It found `iconPlacement`, `iconHidden`
and `embeddedIconOrder`; the generated elevated script was rendered and parsed
without elevating, to check the list reached it.

---

## 2026-09-22 - Choose the overflow, move icons fast, and stop rebuilding the tray every second

**Impact:** two features and a fix that is probably why taskbar dragging never
worked.

**What changed**

* Icons can be put in the secondary tray's overflow (the chevron popup) on
  purpose: "Hide in the overflow menu" / "Show on the tray" on Shift+right-click,
  "Show every hidden icon" to undo them all. Remembered like placements.
* The arrange window has three lists - primary tray, secondary tray, secondary
  tray's overflow. Select an icon and press 1, 2 or 3 to send it there; holding
  the key works down the list. Double-click and Enter move between the trays.
* Shift+left-click an icon in the secondary tray sends it to the primary tray.
* The tray is updated in place when only an icon's picture or tooltip changed,
  and is never rebuilt while the pointer is down.

**Why the overflow was not choosable**

Which icons went into the chevron was decided purely by count: the first
`maxVisibleIcons` were shown and the rest overflowed. `SplitBarAndOverflow`
applies the user's choice first and the count second, so the row limit only
decides among the icons the user wants shown - it no longer spends a slot on an
icon they asked to hide. It is pure and tested, including against the old
count-only behaviour, which the test rejects.

Hidden is kept separate from placement: an icon is in a tray, and within the
secondary tray it is on the bar or in the overflow. Moving between the bar and
the overflow in the arrange window does *not* record a placement, because that
would pin an icon the rules put there and it would stop following them.

**The tray was rebuilt several times a second**

`RefreshEmbeddedTray` cleared the panel and rebuilt every cell on every call, and
it is called on every change to any icon in the tray. SystemInformer redraws four
live graphs every second, so the cell under the pointer was being destroyed
several times a second. A destroyed element loses pointer capture, which ends a
press before it can cross the drag threshold; tooltips closed as they opened; and
an open overflow popup was left holding cells whose indices pointed at whatever
icon now had that index, so a click on it could reach a different application.

A refresh now compares the layout it would draw with the one it drew last. If
the same icons are in the same places, each cell's picture and tooltip is updated
where it stands (and the tooltip only when its text changed, since setting one
closes it). The panel is rebuilt only when the layout changes, the overflow popup
is closed when it does, and nothing is rebuilt while the pointer is down - the
refresh is posted to run after the release, so a forwarded click resolves against
the layout that was clicked.

This is the most likely reason dragging on the taskbar did nothing. It has not
been seen working yet.

**Also**

* The hidden set is read on the taskbar thread and written from both the taskbar
  thread and the arrange window's, so it has its own lock, always taken
  innermost.
* "Reset moved icons" now takes `g_mutex`: placements are otherwise only touched
  under it, and the tray thread may be re-resolving routing at that moment.

**Verification**

* Regression 376 checks / 0 failures (was 358), integration 53 / 0, compile
  check clean, source hygiene clean, settings lint 19/19, symbols 5/5.
* Mutation: making `SplitBarAndOverflow` ignore the user's choice fails the
  regression run. The source was restored in a `finally`, and checked.
* Every structural rebuild is now logged, so the in-place path can be confirmed
  live: a line a second would mean it is not being taken.

---

## 2026-09-22 - A way back from a moved icon, and a probe that survives

**Impact:** a feature and a diagnostic.

**What changed**

* "Reset moved icons" in the tray menu: forgets every hand-made placement, so
  the per-process rules decide again.
* `tools/redeploy.ps1 -ClearPlacements` does the same from outside the mod.
* The drag probe logs once per press rather than once per session.

**Why**

There was no way back. Once an icon had been moved, that choice outranked the
rules permanently, and undoing it meant moving every icon back by hand - or
knowing that the mod keeps its storage under
`HKLM\SOFTWARE\Windhawk\Engine\ModsWritable\local@split-tray\LocalStorage`.

Found because the arrange window was driven with synthetic double-clicks during
testing, which left three placements nobody chose. That was an accident, but the
gap it exposed is real.

It is also the end-to-end verification the previous entry said was missing: the
double-clicks moved the icons, wrote `Everything.exe#0=s`, `desk-tray.exe#4=s`
and a GUID-keyed entry to storage, and the tray came back after an Explorer
restart showing 6 icons rather than 4. The move path works.

**The probe was one-shot, which was the wrong shape**

A `static bool` spent on the first drag of a session - very likely one nobody was
capturing - after which every later attempt looks silent for the wrong reason.
Presses on a tray icon are rare, so every one is logged now, with the move line
kept to one per press.

**Verification**

* Regression 358 checks / 0 failures, integration 53 / 0, compile check clean,
  source hygiene clean, settings lint 19/19, symbols 5/5.
* `Test_ForgettingPlacementsGivesTheRulesBack` covers the reset, including that
  it clears storage and not just memory.
* Not yet deployed: the elevation prompt was dismissed.

**Still open**

* Dragging within the taskbar. The last capture recorded no pointer events on a
  tray cell at all, but nobody was dragging during it, so it says nothing yet.

---

## 2026-09-22 - An arrange window, because dragging in the taskbar will not work

**Impact:** a feature, and a probe for the thing that still does not work.

**What changed**

* A window with the two trays side by side. Drag an icon across, or
  double-click it. Opened from the tray's menu, or by double-clicking the
  handle on an empty tray.
* Icons in the primary tray keep their icon as well as their tooltip, so that
  window can show them.
* One-shot logging on the first pointer press and the first pointer move over a
  tray cell.

**Why a separate window**

Dragging inside the taskbar has now failed twice: once because the dragged cell
was being removed from the panel and losing pointer capture, which was a real
defect and is fixed, and once after that fix with no visible change. Two
attempts is enough to stop guessing at it.

The mod's cells live in Explorer's XAML island, where the pointer does not
behave the way it does in an ordinary window. The arrange window is the mod's
own, on the mod's own thread, with nothing else laying claim to its input, so a
drag there is just a drag. It also moves icons in *both* directions, which
dragging on the taskbar could not: there is no mod-owned element on the primary
taskbar to drag from.

The probe stays in, so the next attempt at the taskbar drag starts from evidence.
Three explanations are indistinguishable without it - the press never arrives,
the press arrives and no moves follow, or moves arrive and the threshold is
never crossed. The first two would be the taskbar taking the input; the third
would be ours.

**No drag image, deliberately**

The first version used `ImageList_BeginDrag`/`DragEnter` for the usual
drag-image effect. `ImageList_DragEnter` with a null window locks the screen DC,
and a drag that cannot finish takes the whole desktop with it - which happened
here under synthetic input during testing. A cursor change conveys the same
thing and cannot wedge anything. The drag also ends itself if the mouse button
is found to be up, and on `WM_CANCELMODE`, `WM_CAPTURECHANGED` and `WM_DESTROY`.

**Verification**

* Regression 355 checks / 0 failures, integration 53 / 0, compile check clean,
  source hygiene clean, settings lint 19/19, symbols 5/5.
* Live: the window opens with the right contents - 20 icons listed under
  Primary tray, 4 under Secondary tray, matching the four SystemInformer icons
  the routing fix put there.
* Not verified live: the move itself, by either gesture. Driving it with
  synthetic messages wedged the drag (see above) and an earlier attempt using
  `LVM_GETITEMRECT` across a process boundary - a message that carries a
  pointer - crashed Explorer. Both were faults in the check, not the mod;
  `MoveIconToTray` underneath is covered by tests.

**Still open**

* Dragging within the taskbar itself. The probe will say why next time.

---

## 2026-09-22 - Why the second tray was empty

**Impact:** three fixes, one of which is why nothing appeared at all.

**What changed**

* Routing is decided from a message that actually carries an executable path,
  not from whichever message arrives first.
* The tray attaches by walking down from the taskbar's `XamlRoot`, retried on
  the timer, instead of only through the `IconView` constructor hook.
* A settings change no longer discards icons the user moved by hand.
* An all-zero GUID is no longer treated as an icon's identity.
* Every `NIM_ADD`/`NIM_DELETE` and every routing decision is logged.
* An empty tray shows a handle, whose menu can move any known icon into it.

**Why nothing was in the tray**

`SystemInformer.exe -> secondary` was seeded, loaded and matched correctly, and
one of the four icons moved. The other three did not, whatever the rule said.

Routing is decided once per icon and is then sticky (`DECISIONS.md` 5). The
decision was being taken on the *first message seen about an icon*, which is not
always its `NIM_ADD`. SystemInformer redraws four live graphs every second, so
for uIDs 3, 5 and 14 the mod's first sight of them was a `NIM_MODIFY` - and a
modify carries no executable path, so the rules had nothing to match and they
were filed under `defaultTray`. The later `NIM_ADD`, carrying the path, was then
treated as already decided. uID 2 worked only because its add happened to arrive
before its first modify.

An icon whose path is not yet known is now *provisional*, not decided. The first
message carrying a path settles it, and the move is applied through the same
replay a settings change uses, rather than by rewriting the decision mid-message.
The log says so: `settled as secondary once the path arrived (SystemInformer.exe)`.
The same line appears for EarTrumpet and TightVNC, which were arriving the same
way.

**Why the tray was sometimes missing entirely**

The panel was only ever created from the `IconView` constructor hook, which sees
elements built after it is installed and nothing that already exists. Resolving
`SystemTray.dll`'s symbols takes seconds - 8.2s after `Wh_ModInit` on one boot,
15s on another - and if the secondary taskbar finished building first, no element
on it ever reached the mod and no tray was created. It had been working by
winning that race. The tree is now walked downwards from the target `XamlRoot`,
which needs nothing to have happened first, and it is retried every two seconds
until it takes. Both taskbars are on one UI thread (verified: `Shell_TrayWnd` and
`Shell_SecondaryTrayWnd` both report thread 93292), so the retry is posted to the
window the mod already subclasses.

**Two things found on the way**

* `ApplySettingsToTrackedIcons` re-resolved every icon straight from the rules,
  so any settings change threw away every placement the user had chosen -
  directly against `DECISIONS.md` 29. It consults the placement first now.
* `SameIcon` matched on `NIF_GUID` alone. The flag only says the caller set it,
  not that it filled the field in, and applications do set it over an empty GUID;
  every such icon was then the same icon. Not what was wrong here - these four
  GUIDs are real and distinct - but the test shows the old code collapsing four
  icons into one.

**Verification**

* Regression 355 checks / 0 failures (was 334), integration 53 / 0, compile
  check clean, source hygiene clean, settings lint 19/19, symbols 5/5.
* Each of the three fixes has a test that fails without it; the empty-GUID test
  was run against the unfixed code and reported `got 1, want 4`.
* Live, after the fix: all four SystemInformer icons decided `secondary`, and
  `[xaml] secondary tray: 4 icon(s), 4 shown, 0 hidden`.

**Still open**

* Dragging *into* the secondary tray from the primary one. The handle's menu
  moves icons either way meanwhile.

---

## 2026-09-22 - Why dragging did nothing, and the tests placement shipped without

**Impact:** a fix, a feature, and coverage for something that shipped untested.

**What changed**

* Drag to reorder works. The row is rearranged by moving the cells around the
  dragged one rather than moving the dragged one itself.
* Dragging an icon off the row sends it to the other tray, and the choice is
  remembered like any other.
* A drag that ends badly no longer registers as a click.
* `tools/redeploy.ps1 -Setting` accepts `name=value` strings.
* Seven regression tests covering per-icon placement and the reordering.

**Why dragging did nothing**

The reorder removed the dragged cell from the panel and re-inserted it at the
new index. An element that leaves the visual tree loses pointer capture, so XAML
raised `PointerCaptureLost`, the handler tore the drag state down, and the drag
was over after one step - with the pointer still held. The release that followed
saw `dragging == false` and forwarded a click, so the gesture ended by activating
whatever icon it had been dropped on.

Walking the neighbouring cell across the dragged one has the same effect on the
order while leaving the dragged cell in place, so its capture holds for the whole
gesture. The step planner is `SplitTray::PlanCellShift`, kept on the non-XAML
side of the guard so it can be tested; the invariant it has to hold - the dragged
cell is never the one removed - is asserted for all 36 from/to pairs in a row of
six.

Two smaller faults on the same path: `CapturePointer`'s result was discarded, so
a refusal was indistinguishable from a drag nobody attempted, and the chevron
could be displaced from the start of the row because slot 0 was a legal target.

**Placement had no tests**

The previous entry's verification counted 204 checks and reported the harness's
real storage as evidence for placement. It was not: nothing referenced
`MakeStableKey`, `ResolvePlacement`, `RememberPlacement` or `MoveIconToTray`. The
feature reached a live run unmeasured. Now covered: the key prefers the GUID and
ignores the path, a remembered placement outranks the rules, placement survives a
restart through storage, and moving an icon retracts it from the secondary tray
and stays moved. `ResetStore` clears remembered placements and the stored string,
which it did not - a leftover would have silently overridden the routing a later
test was asserting.

**Verification**

* Source hygiene clean; settings lint 19/19, 17 of 17 seeded; symbol check 5/5.
* Regression 334 checks / 0 failures (was 204), integration 53 checks / 0
  failures, compile check clean, DLL linked with the mod id confirmed.
* `ConvertTo-SettingTable` exercised against all five argument forms, including
  the `@{...}`-collapsed-to-a-string one that failed.

**Still open**

* Dragging *into* the secondary tray from the primary one needs a mod-owned
  element on the primary taskbar. Out of the tray works; back in is the context
  menu or a rule.
* None of this has been seen running yet.

---

## 2026-09-22 - Two trays rather than a mirror, and the tray's own menu

**Impact:** breaking (the routing model changes), plus a feature and two fixes.

**What changed**

* Per-icon placement, persisted in the mod's storage and consulted before the
  per-process rules.
* `MoveIconToTray` moves one icon between trays on demand and remembers it.
* The tray's own context menu on Shift+right-click: move this icon to the other
  tray, and reset the icon order.
* The overflow flyout no longer vanishes, and has padding.
* `tools/install.ps1` seeds `SystemInformer.exe -> secondary` on a fresh install.
* `tools/check-sources.py` - new, and run first in the build.

**Why the model changed**

Reported: *"should not mirror the tray, should be 2 separate trays that I can
move icons between and remembers the choice"*.

That is a correction to what the mod was, not a new feature on top of it.
`defaultTray: both` makes the secondary tray a copy of the primary one - which is
what has been running, because `both` is non-destructive and was convenient to
test with, and then everything since was built on top of that choice without
revisiting it.

Placement is now per icon. The per-process rules decide where an icon starts;
anything moved by hand overrides them and persists. The key is the icon's GUID
where it has one, otherwise the executable file name and `uID` - never the window
handle, which is a fresh value every launch and would forget the choice exactly
when it matters, on the next restart of the application that owns it.

Falling back to one tray when the second display is absent already worked and is
covered by `Test_MissingSecondaryMonitorFallsBackToPrimary`; a single-display
machine is the same path, since `SelectSecondaryMonitor` finds no non-primary
monitor.

**Why the flyout was flashing**

It was built on the stack inside the handler that showed it, so it went out of
scope as the handler returned. It is now held in a global, shown with
`FlyoutShowMode::Standard` so the pointer sequence that opened it does not
immediately light-dismiss it, and its content is wrapped in a padded border -
the presenter draws the chrome but does not pad arbitrary content.

**Source hygiene**

Three separate corruptions got into this repository through shell heredocs
collapsing backslash escapes: `\b` became a backspace byte in `CHANGES.md`, and
`\0` became a NUL byte inside a character literal - where it has the right value,
so the code compiled, worked, and the file was quietly corrupt. `check-sources.py`
now fails the build on any of them, with `--fix` to repair. C/C++ content with
escapes is written with the file tools from here on, not through a heredoc.

**Verification**

* Source hygiene: found and repaired 2 NUL bytes, then clean across 41 tracked
  files.
* Settings lint 19/19 with 17 of 17 scalars seeded; symbol check 5 of 5.
* Full build, 202s: regression 204 checks / 0 failures, integration 53 checks / 0
  failures, DLL linked with the mod id confirmed.
* The test harness gained a real in-memory implementation of the mod's storage
  rather than a no-op, so placement and order round-trip through it.

**Still open**

* Drag to reorder is reported as not working; not diagnosed yet.
* Dragging *between* the two trays needs the mod to own a panel on the primary
  taskbar as well, so there is something to drop onto. The context menu gives the
  same capability in the meantime.

---

## 2026-09-22 - Overflow chevron, drag to reorder, and the clock stops overlapping

**Impact:** feature plus bug fix.

**What changed**

* Overflow: past `maxVisibleIcons` (new setting, default 8, 0 for all) the rest
  move behind a chevron with a flyout, as the native tray does.
* Drag to reorder, with the order persisted in the mod's own storage.
* `FindTrayRow` rewritten: it now collects the whole ancestor chain, skips past
  the tray button's own template, and requires a `StackPanel`.

**Why the clock was overlapping**

Reported after the previous fix: the icons were no longer inside the clock, but
the clock was now *behind* them rather than to their right.

The row finder had settled on a `Grid`. A Grid puts every child in the same cell
unless told otherwise, so inserting there stacks the mod's panel on top of the
clock instead of beside it - the icons were in the right container and the wrong
layout.

The finder now collects the full ancestor chain first and decides with all of it
in view, rather than returning the first thing that looked plausible. It skips
past the highest `SystemTray.OmniButton` - the clock's own button template, which
is what the first version inserted into - and then takes the first `StackPanel`
above it, because that is the panel that lays children out in a line. If there is
no StackPanel at all it falls back to any panel and says in the log that the
result may overlap, rather than doing it silently.

The chain is printed once, so a third wrong answer reports what the walk had to
choose from instead of needing another round trip to find out.

**Ordering**

The order is keyed on something that survives the owning application restarting:
its GUID where it has one, otherwise the executable name and `uID`. A window
handle would not - it is a fresh handle every launch, so the order would be
forgotten each time the application came back.

It lives in the mod's own storage, not its settings: it is state the mod
maintains rather than something to hand-edit. Icons that are not on screen -
overflowed, or their application is not running - keep their saved position
instead of being dropped. Icons never moved keep the order they registered in,
via a stable sort.

A drag is not also a click: forwarding one on release would activate whatever
icon was dropped on. The threshold is 5 DIP so an unsteady click is still a
click, and a refresh mid-drag is ignored.

**Verification**

* Settings lint: 19 declared, 19 read, 17 of 17 scalars seeded.
* Symbol check: 5 of 5 present.
* Full build, 167s: regression 204 checks / 0 failures, integration 53 checks / 0
  failures, DLL linked with the mod id confirmed.
* **Not yet seen running.** Whether the StackPanel rule lands the icons beside
  the clock, whether the flyout opens, and whether a drag survives a refresh are
  all open until it runs.

**Note**

Two functions were lost to over-wide edits during this change - `RefreshEmbeddedTray`'s
predecessor and then `EnsureEmbeddedPanel` - and both were caught by the build
rather than by review. The second was recovered from the previous commit.

---

## 2026-09-22 - The icons went inside the clock; and they are clickable now

**Impact:** bug fix plus feature. The icons are siblings of the clock instead of
its children, and respond to the mouse.

**What changed**

* `FindTrayRow` replaces `PanelAncestorOf`: it walks up to the tray *row* and
  reports the child it came through, so the mod's panel is inserted in front of
  the clock rather than inside it.
* `AttachCellHandlers`: hover, press, left/right/middle click and double click,
  feeding the click forwarding that section 7 already implements.

**Why**

Reported: the icons were in the clock's container and the clock's context menu
covered all of them.

The cause was the container walk taking the anchor's *nearest* Panel ancestor.
The anchor on a secondary taskbar is the clock, and its nearest Panel ancestor is
inside the clock's own button - so the icons became children of the clock and
inherited its input handling along with its menu.

What is wanted is the row the clock button itself sits in. Walking up while
keeping hold of the child it came through gives both: the row to insert into, and
the clock's top-level wrapper to insert in front of, which puts the icons to its
left where the native tray is. The row is recognised by the names Explorer gives
it and by the `SystemTray.Stack` family, not by counting levels. If neither is
found it falls back to the outermost panel seen rather than the nearest: too high
is recoverable, inside the clock is not.

The pointer events are marked handled, which is the other half of the same
problem - an unhandled press bubbled to the taskbar underneath and opened the
clock flyout.

Clicks use `GetCursorPos` rather than the pointer args' position: the tray
callback protocol wants screen coordinates, and the args give island-relative
device-independent ones that would have to be converted back.

**Verification**

* Full build, 203s: symbol check 5 of 5, settings lint 16 of 16 seeded,
  regression 204 checks / 0 failures, integration 53 checks / 0 failures, DLL
  linked with the mod id confirmed.
* **Not yet seen running.** Whether `FindTrayRow` picks the row rather than
  something higher, and whether clicks reach the owning applications, is open
  until it runs.

**Still not done**

The overflow chevron, drag-to-reorder and the tray's own context menu, all of
which were asked for. They are next, and they are additions to the panel rather
than changes to any of the above.

---

## 2026-09-22 - Size the embedded icons from the anchor's height, not its width

**Impact:** bug fix (user-visible). Embedded icons rendered at roughly twice the
native size.

**What changed**

* `EnsureEmbeddedPanel` derives the cell and icon size from the anchor element's
  **height** and the native proportions, instead of taking its width and halving
  it.
* The insert log line now reports the anchor's class and its measured size.

**Why**

The anchor is whatever tray element loads first on the target taskbar. On a
secondary taskbar that is the **clock**, which is about 73x38 DIP - a wide
element, not a square icon cell. Taking `width * 0.5` therefore asked for a 36
DIP icon where the native one is 16.

The heights do correspond: every element in the tray row is the same height, so
the anchor's height is the row height and the native proportions - a 16 DIP icon
in a 32x38 DIP cell - can be scaled off it. The scale is clamped to [0.5, 3] so a
taskbar resized by another mod still works while a nonsense value cannot.

Worth recording, because it was the second mistake of the same kind: XAML works
in device-independent pixels, so these are the same numbers on a 96 DPI and a 120
DPI monitor. The earlier UIA measurements (32x38 primary, 40x48 secondary) are
*physical* pixels and differ only because the monitors scale differently. Mixing
the two coordinate systems is what produced the wrong number both times.

**Verification**

* Arithmetic against the measurements already taken: primary clock 73x38 physical
  at 96 DPI = 73x38 DIP; secondary clock 91x48 physical at 120 DPI = 72.8x38.4
  DIP. Same element, same DIP size, which is what confirms the height is the
  reliable term and the width is not.
* Full build, 162s: symbol check 5 of 5, settings lint 16 of 16 seeded, regression
  204 checks / 0 failures, integration 53 checks / 0 failures, DLL linked with the
  mod id confirmed.
* The anchor's measured size is now in the log, so a wrong size reports its own
  cause rather than needing to be guessed at again.

---

## 2026-09-22 - The tray goes into the taskbar

**Impact:** feature. The secondary tray is an element inside the secondary
taskbar; the floating panel hides while it is up, and returns if the embedding
cannot be established.

**What changed**

* `src/split-tray.wh.cpp` Section 10: `EnsureEmbeddedPanel`,
  `RefreshEmbeddedTray`, `OnIconStoreChanged`, `RemoveEmbeddedPanel`.
* The `Shell_TrayWnd` subclass refreshes the embedded tray when the icon store
  changes.
* `ApplyLayoutToWindow` hides the floating window while the embedded tray is up.

**Why this way**

The insertion point is derived at runtime rather than written down. From a tray
element already known to be on the target taskbar, the mod walks up to the first
XAML `Panel` that can hold children and inserts its own `StackPanel` at the
front. That lands the icons to the left of the clock, where the native tray sits,
without depending on the shape of Explorer's private tree - which was the thing a
tree dump was going to be needed for.

Sizes are measured, not assumed. The anchor element's `ActualWidth` and
`ActualHeight` are the real cell size in DIPs, already correct for the monitor's
scaling and for whatever other taskbar mods have done to the tray. The earlier
measurement of 32x38 at 96 DPI is a fact about one machine at one moment;
reading it off the live element is a fact about the taskbar it is being inserted
into.

Icon changes update the XAML directly from the `Shell_TrayWnd` subclass, with no
marshalling, because that handler already runs on the taskbar's UI thread - the
same thread that owns the XAML. This was established earlier: both taskbars and
their islands are on one thread.

The panel is removed on unload, so unloading the mod does not leave a stray
element in the taskbar until the next Explorer restart - the same principle as
replaying swallowed icons back into the shell.

**Verification**

* Symbol check against the live binaries: 5 of 5.
* Settings lint: 18 declared, 18 read, 16 of 16 scalars seeded.
* Compile check in real mod mode: clang exit 0.
* Full build, 211s: regression 204 checks / 0 failures, integration 53 checks / 0
  failures, DLL linked, mod id confirmed in the binary.
* **Not yet seen running.** Whether the hooks resolve, whether the anchor's
  parent panel is the right container, and whether the icons render at a sensible
  size are all open until it runs in Explorer.

**Not done yet**

Clicks, hover states, the overflow chevron, drag-to-reorder and the context menu.
The icons render and carry their tooltips; they are not yet interactive. Click
forwarding itself is built and tested - it is the pointer handlers on the XAML
elements that are missing.

---

## 2026-09-22 - The symbol hooks

**Impact:** breaking (Section 10 is replaced). `embedInTaskbar` is on again. The
floating tray and everything below the presentation layer are untouched.

**What changed**

* `src/split-tray.wh.cpp` Section 10 rewritten.
  * Removed: the TAP, `VisualTreeWatcher`, `InitializeXamlDiagnosticsEx`, island
    tracking, and the `DllGetClassObject` / `DllCanUnloadNow` exports that only
    existed for XAML diagnostics to load the DLL by.
  * Added: symbol hooks into `SystemTray.dll` and `taskbar.dll`, the
    window-to-`XamlRoot` resolution, and the element matching.
* `embedInTaskbar` defaults to `true` again.

**How it works now**

`SystemTray.dll`'s `IconView` constructor is the anchor. Every tray icon view
Explorer creates runs through it, and the XAML element is the implementation
object's projected interface:

```cpp
slots[1]->QueryInterface(winrt::guid_of<wux::FrameworkElement>(),
                         winrt::put_abi(iconView));
```

`XamlRoot` is not available until an element is in a tree, so the work waits for
`Loaded`, with the revoker held in a list - a bare token would outlive the
element and fire on a dead object.

Matching an element to its taskbar goes **window to XamlRoot**, not the reverse:
`Shell_SecondaryTrayWnd` -> its `WorkerW` child -> the `CSecondaryTaskBand`
sub-object whose vftable is the one for `ITaskListWndSite` -> `GetTaskbarHost`
-> the host's root element -> `XamlRoot`. Elements are then matched by identity.
The host keeps that element at an offset that is no part of any contract, so it
is read out of the first instructions of `TaskbarHost::FrameHeight`, which opens
by loading that very member, rather than hardcoded.

Hook installation is retried from the tray thread's existing timer. Neither
`SystemTray.dll` nor `taskbar.dll` is necessarily loaded when Windhawk injects,
for the same reason the taskbar window does not exist yet (`DECISIONS.md` 18),
and hooks registered after `Wh_ModInit` need `Wh_ApplyHookOperations`.

**What it does so far**

Observes and reports. For each tray element on the target taskbar it logs the
class name, the element name, and which named stack it landed in - `MainStack`,
`NonActivatableStack`, `ControlCenterButton` or `NotificationCenterButton`. With
`dumpXamlTree` on it prints the subtree once. Nothing is inserted yet: the
insertion point is to be chosen from that output rather than assumed.

**Verification**

* Symbol check against the live binaries: 5 of 5 present.
* Settings lint: 18 declared, 18 read, 16 of 16 scalars seeded.
* Compile check in real mod mode (no `WH_EDITING`): clang exit 0, no
  diagnostics.
* Full build, 279s: regression 204 checks / 0 failures, integration 53 checks / 0
  failures, DLL linked with the mod id confirmed in the binary.
* **Not yet run in Explorer.** Whether the hooks resolve at runtime, and whether
  the `XamlRoot` resolution returns the right one, is not established by any of
  the above.

---

## 2026-09-22 - Ground the symbol-hook approach against the live binaries

**Impact:** tooling; no runtime change yet. This is the groundwork the embedding
is written against.

**What changed**

* `tools/check-symbols.py` - new. Resolves each module's PDB the way Windhawk
  does (CodeView debug directory -> GUID+age -> local cache, else the public
  symbol server), dumps its public symbols with `llvm-pdbutil`, and checks that
  every mangled name the mod hooks is present. `--offline` for a network-less
  build.
* `tools/build.ps1` runs it before the compile check.
* `DECISIONS.md` 26, 27, 28.

**Why**

The reference mod's hook carries the comment `// SystemTray.dll,
Taskbar.View.dll` - two candidate modules, no statement of which one holds the
symbol on any given build. Taking that at face value would have meant writing the
hook against `Taskbar.View.dll`, which on this machine contains **no `IconView`
symbol at all** - not in publics, not in globals.

Checking instead:

| symbol | module | present |
| --- | --- | --- |
| `??0IconView@implementation@SystemTray@winrt@@QEAA@XZ` | SystemTray.dll | yes |
| `??_7CSecondaryTaskBand@@6BITaskListWndSite@@@` | taskbar.dll | yes |
| `?GetTaskbarHost@CSecondaryTaskBand@@UEBA?AV?$shared_ptr@VTaskbarHost@@@std@@XZ` | taskbar.dll | yes |
| `?FrameHeight@TaskbarHost@@QEBAHXZ` | taskbar.dll | yes |
| `?_Decref@_Ref_count_base@std@@QEAAXXZ` | taskbar.dll | yes |

`SystemTray.dll` also carries `Stack`, `NotifyIconView`, `SystemTrayFrame`,
`NotificationAreaIcons`, `NotificationAreaOverflow`, `ChevronIconView`,
`SystemTrayController` and `SystemTraySecondaryController` - the last of which
says Explorer already models the secondary taskbar's tray as its own controller.

This also settles the open question from the previous entry. Matching an element
to its taskbar cannot go element -> window without diagnostics, but it can go
window -> `XamlRoot`: `Shell_SecondaryTrayWnd` -> its `WorkerW` child ->
`CSecondaryTaskBand` -> `GetTaskbarHost` -> the host's element -> `XamlRoot`,
then compare by identity. Exact, and it retires the size-and-scale heuristic,
which only separated the two taskbars here because the monitors happen to differ.

**Verification**

* Run against the live binaries: 5 of 5 symbols present, `SystemTray.dll` (20215
  public symbols, downloaded), `taskbar.dll` (22104, cached).
* Teeth check: renaming one entry to
  `?FrameHeight@TaskbarHost@@QEBAHXZ_RENAMED_BY_A_WINDOWS_UPDATE` makes it exit 1,
  print `MISS`, and name what the symbol was needed for.

**Regression anchor**

`tools/check-symbols.py`, run before every compile: a hooked symbol that is no
longer in the live binaries fails the build instead of the mod.

---

## 2026-09-22 - XAML diagnostics is the wrong mechanism; it has one consumer per process

**Impact:** bug fix. `embedInTaskbar` now defaults to off, so the mod stops
asking for a resource it cannot have. The floating tray is unaffected.

**What changed**

* `embedInTaskbar` defaults to `false` until the mechanism is replaced.
* `DECISIONS.md` 24 and 25 record the correct mechanism and the element names.
* Decision 22's goal stands; its mechanism is superseded.

**Why**

Attaching produced a dialog from another mod:

> The following module is trying to use XAML diagnostics: ...
> There can only be one consumer at a time. Blocking it might break that module,
> but allowing it might break this mod.
> -- Windows 11 Taskbar Styler - Windhawk

`windows-11-taskbar-styler` is installed and enabled here, holds the XAML
diagnostics connection, and hooks `InitializeXamlDiagnosticsEx` to challenge any
other caller. So the choice the design forced was: break the styler, or do not
embed.

That was an incorrect assumption on my part, stated explicitly earlier in the
work - that several TAPs coexisting was the normal case. It is not:
`InitializeXamlDiagnosticsEx` is single-consumer per process.

Checking what the mods that actually manipulate SystemTray elements do settles
it. Neither uses diagnostics:

| mod | InitializeXamlDiagnosticsEx | Taskbar.View.dll symbol hooks |
| --- | --- | --- |
| `taskbar-tray-system-icon-tweaks` | 0 | 2 |
| `taskbar-clock-customization` | 0 | 2 |
| `windows-11-taskbar-styler` | holds it | 0 |

They hook `winrt::SystemTray::implementation::IconView::IconView`, which every
tray icon passes through, and take the XAML element from the implementation
object's projected interface:

```cpp
((IUnknown**)pThis)[1]->QueryInterface(winrt::guid_of<FrameworkElement>(),
                                       winrt::put_abi(iconView));
```

That needs no exclusive connection and coexists with the styler. It also names
the containers the insertion point was going to be hunted for: `MainStack`,
`NonActivatableStack`, `ControlCenterButton`, `NotificationCenterButton`, with
icon views named `SystemTrayIcon`.

**Still open**

Mapping an element to *which* taskbar it belongs to. The diagnostics design did
this by tracking `DesktopWindowXamlSource` roots and their HWNDs; without
diagnostics those are not reported. `XamlRoot` size and rasterization scale
distinguish the two taskbars on this machine (1536x48 DIP at 1.25 versus 1920x48
DIP at 1.0) but would tie on two identical monitors, so it is a heuristic rather
than an answer. To be settled before the insertion is written.

**Verification**

* Settings lint: 18 declared, 18 read, 16 of 16 scalars seeded.
* Not a code path change beyond the default - the diagnostics code remains in the
  tree, unreferenced at runtime, until it is replaced.

---

## 2026-09-22 - The installer stopped seeding new settings, which switched the XAML attachment off

**Impact:** bug fix (user-visible). The XAML attachment never ran at all.

**What changed**

* `tools/install.ps1` - `Get-DefaultSettings` now parses the mod's own
  `==WindhawkModSettings==` block instead of carrying a hardcoded table, and the
  install verifies afterwards that every declared scalar is actually present in
  the registry, throwing if not.
* `tools/check-settings.py` - third check: re-runs the installer's parsing rule
  over the block and fails if any declared scalar would not be seeded, naming a
  folded scalar as the reason when that is why.

**Why**

A live run produced a healthy-looking log with no `[xaml]` line in it at all -
not the success branch, not the failure branch:

```
Wh_ModInit: Split Tray initialising
...
SubclassShellTrayWindow: subclassed Shell_TrayWnd 0000000006451660
RequestIconRepopulation: broadcast TaskbarCreated to collect existing icons
```

Reading the registry rather than guessing showed why: of the 16 scalar settings
the mod declares, `embedInTaskbar` was absent. The engine reads settings from the
registry, and a value that is not there comes back as 0 - so `embedInTaskbar`
read as off, `AttachToXaml()` was never called, and the entire XAML section sat
there doing nothing without emitting a line.

The cause was `tools/install.ps1` carrying its own hardcoded copy of the
defaults, which went stale the moment `embedInTaskbar` and `dumpXamlTree` were
added to the mod. Windhawk's UI seeds defaults itself; a hand install has to, and
nothing was checking that it still knew the full list.

This is the same class of defect the settings lint was written for - a setting
that silently reads back as 0 - one level further out: the lint checked
code against the block, and nothing checked the block against the installer.

**Verification**

* Lint on the real mod: 18 declared, 18 read, **16 of 16 scalars seeded**.
* Teeth check: turning `opacity` into a folded scalar the installer's regex
  cannot read makes the lint exit 1 with
  `- opacity (folded scalar)`.
* The installer's parser, run against the real mod, returns all 16 scalars with
  correct types, including `embedInTaskbar = 1`.
* `install.ps1` and `redeploy.ps1` both parse clean.

**Regression anchor**

`tools/check-settings.py`, run as the first step of every build: a declared
scalar the installer cannot seed fails the build.

---

## 2026-09-22 - Keep the XAML section out of the test binaries

**Impact:** build tooling only; the shipped mod is byte-for-byte unaffected.

**What changed**

* Section 10 is wrapped in `#ifndef SPLITTRAY_NO_XAML`, and the two lifecycle
  call sites with it.
* The regression, integration and mutation builds define `SPLITTRAY_NO_XAML`.
  The compile check and the DLL build deliberately do not.

**Why**

A full `tools\build.ps1` had gone from about 90 seconds to **10 minutes**: the
mod now includes nine WinRT projections, and every one of the four compiles in
the loop paid for them. The test binaries cannot exercise any of Section 10 -
there is no XAML island in a plain test process and no Explorer taskbar to attach
to - so they were compiling it purely to pay for it.

A full run is now **159 seconds**, with the same 204 regression checks and 53
integration checks passing.

This is not a test double standing in for the real thing, which
`DECISIONS.md` #21 rules out: the code is absent from the test binary rather than
replaced by something that pretends to work. The two builds that decide whether
the shipped artefact is correct - the compile check under Windhawk's own editor
flags, and the DLL link - still compile it in full, so nothing about Section 10
goes unchecked.

**Verification**

* `tools\build.ps1`: settings lint 18/18, compile check clean, regression 204
  checks / 0 failures, integration 53 checks / 0 failures, DLL linked with the mod
  id confirmed present in the binary, 159s wall clock.

---

## 2026-09-22 - Icon pixels for XAML, and both taskbars in the tree dump

**Impact:** additive; nothing is wired up to it yet.

**What changed**

* `src/split-tray.wh.cpp`
  * `ReadIconPixels` / `IconToBitmap` - convert an `HICON` into a
    `WriteableBitmap`.
  * `FindDescendantByType` / `FindInsertablePanel` - locate containers by runtime
    class name rather than by a hardcoded path through Explorer's private tree.
  * The tree dump now prints the tray frame subtree of **both** taskbars.

**Why**

The floating renderer handed an `HICON` straight to `DrawIconEx`. XAML needs
pixels, and the conversion is where this goes wrong quietly: tray icons come in
two historical shapes, and getting the alpha wrong shows up as a black box
around every icon.

* A 32-bit icon carries its own alpha channel in the colour bitmap.
* An older icon has none; its transparency lives in a separate 1bpp mask where a
  set bit means transparent.

`GetDIBits` reads the colour bitmap, and if every alpha byte comes back zero the
alpha is rebuilt from the mask. The result is premultiplied, because
`WriteableBitmap` treats its buffer that way - handing it straight alpha leaves a
dark fringe on every anti-aliased edge.

Both taskbars are dumped because the primary one is the reference: it is the only
one with a real notification area, so its subtree is the only place to see which
element types hold tray icons and how they nest. Each dump costs an Explorer
restart to collect, so one run prints both.

**Notes**

* `winrt/Windows.Storage.Streams.h` has to be included before
  `WriteableBitmap::PixelBuffer()` is used. `IBuffer`'s accessors have deduced
  return types, so the consumer definitions must be in scope, not just the
  forward declaration the imaging projection pulls in.

**Verification**

* Compile check in real mod mode (no `WH_EDITING`, which is the mode that caught
  the missing include): clang exit 0, no diagnostics.
* Full build: settings lint 18/18, regression suite, integration suite, DLL link
  with the mod id confirmed present in the binary.

**Correction**

An earlier report in this session that the icon converter "compiles clean" was
wrong. The check behind it had not run: the watcher script tested for a running
compiler process, found none because clang had not started yet, and read an empty
log as success. The two errors above were sitting in the file at the time. The
compile check now captures clang's own exit code directly rather than inferring
it.

---

## 2026-09-22 - Stop passing quoted macros through a shell

**Impact:** build tooling only; the mod source is unchanged. Windhawk's own
compilation was never affected - only the out-of-band DLL build in this repo.

**What changed**

* `tools/build.ps1` writes `build/mod_defines.h` with `WH_MOD_ID` and
  `WH_MOD_VERSION` and passes it with `-include`, instead of putting
  `-DWH_MOD_ID=L"..."` on the command line.
* After linking, the built DLL is searched for the mod id as a UTF-16 string and
  the build fails if it is absent.

**Why**

`tools\build.ps1 -SkipTests` succeeded under PowerShell 7 and failed under
Windows PowerShell 5.1 with:

```
#define WH_MOD_ID Llocal@split-tray
```

The two editions hand quotes to native commands differently, so under 5.1 the
quotes were stripped and the macro arrived as a bare token. It only surfaced
where the macro is concatenated with another literal - in `windhawk_utils.h` and
in the mod's own `GetReplayMessage()` - which made it look like a source problem
rather than a build one. A generated header takes the shell out of the path
entirely.

The added check exists because the failure mode one step milder than this - a
macro that expands to the *wrong* string rather than to nothing valid - would
link cleanly and only show up as two mods fighting over a registered message
name at runtime.

**Verification**

* `tools\build.ps1 -SkipTests` run under Windows PowerShell 5.1, the edition that
  failed: clean compile check, DLL links, all five lifecycle exports present, and
  `mod id 'local@split-tray' present in the binary`.

---

## 2026-09-22 - Attach to Explorer's taskbar XAML (stage 1 of the embedding)

**Impact:** additive. The floating tray is unchanged; this adds the attachment the
embedded tray will be built on, and a diagnostic that prints the tree it targets.

**What changed**

* `src/split-tray.wh.cpp` - new Section 10, XAML attachment:
  * A TAP (`CLSID_SplitTrayTAP`) registered through `InitializeXamlDiagnosticsEx`,
    with `DllGetClassObject` / `DllCanUnloadNow` exports for COM to load it by.
  * A `VisualTreeWatcher` implementing `IVisualTreeServiceCallback2`, advised from
    its own thread.
  * Island tracking: every `DesktopWindowXamlSource` is recorded with the HWND
    behind it, and an element is matched to one by `XamlRoot` identity.
  * `SystemTray.SystemTrayFrame` elements are picked up on `Loaded` and matched
    to the taskbar window that owns them, so the one on the configured monitor
    can be identified.
  * `DumpSubtree` prints that frame's subtree when `dumpXamlTree` is on.
* Two new settings: `embedInTaskbar` (default on) and `dumpXamlTree` (default
  off).
* `tools/build.ps1` now takes the link libraries from the mod's own
  `@compilerOptions` line instead of repeating them, and sets
  `$PSNativeCommandUseErrorActionPreference = $false`.
* `tools/redeploy.ps1` - new. One iteration of the live loop (build, elevated
  install, Explorer restart, log capture) in a single step, because each
  iteration costs a UAC prompt and takes the taskbar down.

**Why**

Both taskbars run on the same UI thread (2528 when measured), so the thread an
element arrives on says nothing about which monitor it belongs to. Matching an
element to its taskbar therefore has to go through the XAML island's native
window, which is why island tracking exists before anything is inserted.

The tree dump exists because Explorer's taskbar XAML is undocumented and changes
between builds. Choosing insertion points from a printed tree is checkable;
choosing them from assumption is not.

**Notes**

* Nothing is released on the `Remove` callback. That report arrives from inside
  XAML's own Leave walk over the subtree being torn down, so dropping the last
  reference there can destroy an element underneath the walker. References taken
  to report an element are handed back on `Add` instead.
* `#undef GetCurrentTime` before the WinRT XAML headers: `winbase.h` defines it as
  a macro and it collides with `Timeline::GetCurrentTime`.

**Verification**

* Settings lint: 18 declared, 18 read, in agreement.
* Compile check with Windhawk's editor flags: clean.
* Regression suite: 204 checks, 0 failures. Integration suite: 53 checks, 0
  failures. Both compile the mod source including Section 10.
* Mod DLL links and exports all five lifecycle entry points plus
  `DllGetClassObject` and `DllCanUnloadNow`.
* **Not yet run in Explorer** - the elevated install was cancelled at the UAC
  prompt, so the tree dump has not been captured and no insertion point has been
  chosen yet.

---

## 2026-09-22 — Keep looking for the taskbar; clear the unload flag on load

**Impact:** breaking fix. Before this the mod did nothing at all on a normal boot.

**What changed**

* `src/split-tray.wh.cpp`
  * New `EnsureShellTrayWindowSubclassed()`, called from `Wh_ModAfterInit` **and
    from the tray thread's timer**, which keeps looking for `Shell_TrayWnd` until
    it finds one and re-attaches if the taskbar is destroyed and recreated.
    `SubclassShellTrayWindow()` now reports whether *this* call attached.
  * `RequestIconRepopulation()` moved out of `Wh_ModAfterInit`: the
    `TaskbarCreated` broadcast now happens only once the subclass is actually in
    place.
  * `Wh_ModInit` clears `g_unloading`.
* `tests/integration/mod_integration_test.cpp` — new phase `[11]`: load the mod
  with no tray window in existence at all, then create one, and assert the mod
  attaches and intercepts. Also re-launches itself as a child process on the
  private desktop.
* `tools/mutate.py` — mutants now declare which suite should kill them, so the
  three defects that only the integration suite can catch are covered.

**Why**

Found by installing the mod in Windhawk and reading its log in a live Explorer:

```
06:20:01.518 explorer.exe [local@split-tray] [Wh_ModInit]: Split Tray initialising
06:20:01.521 ... monitor 1: work area (-1920,0)-(0,1140) dpi=120
06:20:01.524 ... monitor 2: work area (0,0)-(1920,1032) dpi=96 [primary]
06:20:01.531 ... Shell_TrayWnd not found yet, will retry
06:20:01.694 ... broadcast TaskbarCreated to collect existing icons
```

and then nothing, ever. Three separate problems in that one trace:

1. **Windhawk injects into `explorer.exe` before the shell creates its taskbar**,
   so there is no `Shell_TrayWnd` to attach to at `Wh_ModInit`. The code said it
   would retry and the comment on the timer claimed it did — but the timer only
   called `ReplayRoutingChanges()` and `ApplyLayoutToWindow()`. The subclass was
   never installed, so the mod sat there inert with a healthy-looking log. This is
   the failure mode on *every* real boot; only a live run could find it, because
   the integration test happened to create its tray window first.
2. The `TaskbarCreated` broadcast went out anyway, asking every application to
   re-register into a tray the mod was not yet watching — wasting the one chance
   to collect icons that pre-date the mod.
3. `g_unloading` was never cleared on load, so a mod loaded again in the same
   process would disable all of its own hook paths. Windhawk loads a mod once per
   process, so this would not bite in production, but it made the second lifecycle
   in the integration test silently do nothing.

Confirmed on the live machine that `SplitTraySecondaryTray` existed (1x1, hidden,
owned by `explorer.exe`) while `Shell_TrayWnd` was present and unsubclassed — so
the tray thread and window were fine and the attach was the only broken link.

**Verification**

* Integration test: **53 checks, 0 failures**. Phase `[11]` log shows
  `Shell_TrayWnd not found yet, will retry` → `subclassed Shell_TrayWnd` →
  `routed away from primary tray`, i.e. the exact live sequence now completes.
* Mutation check extended to 13 mutants, including "the tray window is never
  looked for again after startup", "a settings change no longer moves icons that
  are already on screen" and "g_unloading is not cleared on load" — each killed
  only by the integration suite, which is why it now runs in the mutation check.

**Regression anchors**

* `tests/integration/mod_integration_test.cpp` phase `[11]` — the mod must attach
  to a tray window that appears *after* it loads.
* `tools/mutate.py` mutant "the tray window is never looked for again after
  startup" — removing the retry must fail the suite.

**Confirmed live**

Rebuilt, reinstalled and enabled in Windhawk, Explorer restarted, log captured:

```
06:38:49.768 ... Shell_TrayWnd not found yet, will retry
06:38:51.843 ... subclassed Shell_TrayWnd 0000000007A81624
06:38:51.849 ... broadcast TaskbarCreated to collect existing icons
06:39:03.946 ... forwarded 0x204 to icon 1  (hWnd=00000000000204E4 uID=2 v4)
06:39:10.572 ... forwarded 0x201 to icon 20 (hWnd=0000000000EB1362 uID=4 v0)
```

The retry attaches one timer tick after the taskbar appears, the repopulation
broadcast follows it rather than preceding it, and clicks on the secondary tray
reach real applications over both the version 4 and the version 0 callback
protocol. The tray window is visible at (-430,1060) 420x70 on the secondary
monitor - 12 columns by 2 rows at that monitor's 120 DPI - showing 21 mirrored
icons.

**Also fixed here**

`tools/install.ps1` used `New-Item -Force` on the mod's registry key, which
recreates an existing key and deletes every value and the whole `Settings`
subkey with it. A reinstall therefore silently reset the user's configuration and
left `Disabled` unset. It now creates the key only when it is missing and carries
the previous enabled state forward. Caught by reading the installer's own status
output after a reinstall reported `disabled` when the mod had been enabled.

**Note on the earlier harness**

The test harness's `SetWindowSubclassFromAnyThread` originally returned `TRUE`
without subclassing anything, which is why the integration test could not have
caught this before. It now mirrors Windhawk's real implementation (a
`WH_CALLWNDPROC` hook on the window's thread plus a registered message). Getting
that working also required running the test as a process started *on* the private
desktop: `SetThreadDesktop` moves only the calling thread, so the mod's tray
thread stayed on the default desktop and the cross-desktop hook was refused with
`ERROR_ACCESS_DENIED`.

---

## 2026-09-22 — Lint the settings block against the code

**Impact:** none (tooling only; no behaviour change).

**What changed**

* `tools/check-settings.py` — new. Validates the `==WindhawkModSettings==` block
  as YAML and checks that the set of settings the code reads is exactly the set
  the block declares.
* `tools/build.ps1` — runs the lint first, before the compile check.

**Why**

A Windhawk setting that the settings block does not declare reads back as `0` or
an empty string, silently: no compile error, no warning, the mod simply behaves
as though the user never changed anything. With 16 settings that is a plausible
way to ship a mod that looks configurable and is not. A YAML syntax error in the
block is similarly unhelpful — it stops the mod compiling in the Windhawk UI with
a message that does not point at the cause.

**Verification**

* Clean run: 16 settings declared, 16 read, in agreement.
* Checked the lint has teeth: renaming one `Wh_GetIntSetting(L"opacity")` call to
  `"opacityy"` makes it exit 1 and name both sides of the mismatch.
* The settings block also parses as YAML with the same array-of-struct shape that
  stock mods such as `file-explorer-remove-suffixes` and
  `taskbar-clock-customization` use for their list settings.

---

## 2026-09-22 — Settings changes now move icons that are already on screen

**Impact:** bug fix (user-visible).

**What changed**

* `src/split-tray.wh.cpp` — `WM_ST_SETTINGS` now calls
  `ApplySettingsToTrackedIcons()` before re-laying out. It previously fell
  through to the same handler as `WM_ST_REFRESH`, which only recomputed geometry,
  so `ApplySettingsToTrackedIcons` was never reached at all.
* `tests/integration/mod_integration_test.cpp` — new. Drives the whole mod with
  the real shell32 producing the messages, on a private desktop, so the live
  shell is never involved.
* `tests/harness/windhawk_utils.h` — `SetWindowSubclassFromAnyThread` now
  performs a real subclass on the calling thread instead of returning `TRUE`
  without doing anything, so the integration test exercises the mod's actual
  `Shell_TrayWnd` handler.
* `tools/build.ps1` — runs the integration test after the unit tests.

**Why**

Adding a routing rule for an application whose icon was already visible did
nothing until that application next touched its icon. The unit tests did not
catch it because they call `ApplySettingsToTrackedIcons()` directly; nothing
checked that the message handler ever called it. The integration test caught it
on its first run.

**Verification**

* Integration test: **44 checks, 0 failures**, covering
  * suppression really removes an icon from the shell (the stand-in Explorer
    receives `NIM_ADD` for a `primary` icon and nothing at all for a `secondary`
    one),
  * the mirrored icon carries the real tooltip and a copied `HICON`,
  * the tray window lands at `(-448, 876) 56x28` inside the work area
    `(-1920,0)-(-384,912)` — a monitor at negative coordinates,
  * a click on a mirrored icon reaches the owning window with `WM_LBUTTONUP`
    (`0x202`) over the real tray callback protocol,
  * a live settings change moves an on-screen icon between trays, and the shell
    is sent the matching `NIM_DELETE`,
  * a `NIM_MODIFY` carrying only `NIF_ICON` keeps the tooltip,
  * unloading replays every swallowed icon back into the shell and removes the
    subclass, after which traffic reaches the shell directly again.
* Regression suite still 204 checks, 0 failures; mutation check still 10/10.

**Regression anchor**

`tests/integration/mod_integration_test.cpp` step `[3]` — asserts that
`Wh_ModSettingsChanged()` alone moves an existing icon and retracts it from the
shell.

---

## 2026-09-22 — Rebuilt the mod on the correct interception point

**Impact:** breaking (complete replacement of the mod's capture mechanism and of
the project layout).

**What changed**

* `src/split-tray.wh.cpp` — new mod, version 1.0.0, replacing every previous
  attempt. Captures tray notifications by subclassing Explorer's
  `Shell_TrayWnd` and parsing its `WM_COPYDATA` payload, instead of hooking
  `Shell32!Shell_NotifyIconW`.
* Implements the original specification: a second tray on a chosen monitor,
  per-process `primary`/`secondary`/`both` routing, a `defaultTray` fallback,
  live settings via `Wh_ModSettingsChanged`, fallback to the primary tray when
  the configured monitor is absent, and clean unload behind an atomic
  `g_unloading` flag.
* `tests/probe/shell32_wire_probe.cpp` — new. Captures the real
  `Shell_TrayWnd` wire format from the live shell32 on a private desktop.
* `tests/regression/split_tray_tests.cpp` — new. 41 tests / 204 checks,
  compiled against the shipped mod source via the stub headers in
  `tests/harness`.
* `tools/build.ps1`, `tools/install.ps1`, `tools/run-tests.ps1`,
  `tools/mutate.py`, `tools/gen-golden.py`, `tools/dbgcapture.cpp` — new.
* `README.md`, `DECISIONS.md`, `CHANGES.md` — new.
* 53 superseded source attempts and 34 progress notes moved to `.archive/`.
  Build artefacts (`*.o`) and `.VSCodeCounter/` removed.

**Why**

The mod did not work, and the reason was architectural rather than a bug to
fix. `Shell_NotifyIcon` executes in the calling application's process; it
reaches the shell only as a `WM_COPYDATA` message to `Shell_TrayWnd`. A hook on
`Shell_NotifyIconW` placed inside `explorer.exe` can therefore only ever see the
icons Explorer itself owns.

`tests/evidence/explorer-hook-log-2025-06-20.txt` shows this directly: over 651
lines the hook captured six tooltips — *Battery Meter*, *Battery status*,
*Bluetooth Devices*, *Safely Remove Hardware*, *The Audio Service is not
running*, *1 - LG TV: 36%* — from three window handles, all Explorer's. No
third-party icon appeared, because none ever passes through that function inside
Explorer. The process-enumeration, UI-Automation and icon-extraction fallbacks
added afterwards are why the mirror filled with unrelated application icons
instead.

A second, independent defect: the mirror window was placed by assuming the
secondary display is to the right of the primary. The secondary display on this
machine is at `x = -1920`, and the old code created its window at `x = 3620` —
off every screen.

**Verification**

* Compile check with Windhawk's own editor flags, read from its
  `compile_flags.txt`: clean, no warnings.
* `tests/probe/shell32_wire_probe.cpp` run on Windows 10.0.26100: confirmed
  `cds.dwData == 1`, signature `0x34753423`, one canonical 1484-byte record with
  `nid.cbSize == 956` for ANSI, Unicode, V1, V2, V3 and V4 callers alike, and
  the sender's exe path at offset `0x3C4`. Raw output kept in
  `tests/probe/probe-output-26100.txt`.
* Regression suite: 41 tests, **204 checks, 0 failures**.
* Mutation check (`tools/mutate.py`): **10/10 mutants killed (100%)**, including
  "secondary icons no longer suppressed", "missing-monitor fallback removed",
  "bottom-right anchor drifts off the monitor", "routing stops being sticky" and
  "szTip read from the wrong offset".
* Mod DLL built with Windhawk's bundled clang: imports `InternalWh_*` from
  `windhawk.dll` exactly as a stock mod does, and exports all five lifecycle
  entry points (`_Z10Wh_ModInitv`, `_Z15Wh_ModAfterInitv`,
  `_Z21Wh_ModSettingsChangedv`, `_Z18Wh_ModBeforeUninitv`,
  `_Z12Wh_ModUninitv`).

**Regression anchors**

* `Test_ParsesRealUnicodeV4Payload`, `Test_ShellNormalisesLegacyAndAnsiCallers` —
  the record the mod must read, pinned to bytes captured from the real shell32.
  These fail if the interception point or the layout is ever wrong again.
* `Test_TrayLandsInsideAMonitorAtNegativeCoordinates` — the off-screen window.
* `Test_MissingSecondaryMonitorFallsBackToPrimary` — specification requirement 7.
* `Test_RoutingIsStickyForTheLifeOfAnIcon`,
  `Test_SecondaryOnlyIconIsSwallowedAndMirrored` — the routing contract.

**Not done**

* No live run inside Explorer yet. That needs the mod enabled in Windhawk and
  Explorer restarted, which is the user's call; `tools/install.ps1` and
  `build/dbgcapture.exe` are in place for it.
* Balloon notifications, UI Automation exposure of the secondary tray, and
  overflow/chevron behaviour are out of scope for 1.0.0 (see `README.md`).
