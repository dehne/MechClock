# Notes While Developing

I've been seeing occasional hangs in stepping the motors. To track the source down I implemented a watchdog in ULN2003Pico's repeating timer callback. It just barked:

```
> status
WiFi is connected, system clock is set, test is off.
At 0:25:27 UTC displayed moon phase is 13/60, actual moon phase is 13/60, next phase change is in 10:05:56.
Watchdog barked. It said: Motor was 0, curMicros was 2595401951, microsNextStep was 3931967850, steps to go was 144
```

Interesting. curMicros is nowhere near crossing 0 (4294967295 --> 0), but microsNextStep is a bizarrely long way in the future.

Added "volatile" to the other variables used in the repeating timer callback. Then, after several cycles without trouble, got:

```
> status
WiFi is connected, system clock is set, test is off.
At 17:01:49 UTC displayed moon phase is 16/60, actual moon phase is 16/60, next phase change is in 4:55:45.        
Watchdog barked. It said: Motor was 0, curMicros was 269542184, microsNextStep was 1994201537, steps to go was 164.
```

Again, very strange. So, I'm wondering if the callback isn't always getting called back in a timely way. So I implemented more watchdog code looking, this time, for a delay in the execution of the callback. Here's what it said:

```
> status
WiFi is connected, system clock is set, test is off.
At 0:07:05 UTC displayed moon phase is 17/60, actual moon phase is 17/60, next phase change is in 9:39:13.
Watchdog barked: Callback delayed. curMicros was 833180873, lastCurMicros 833138087.
```
Yes, it got called back 42786 μs after the last one, not 512 μs plus or minus a few. And in another test it came in at 40947 μs. I've been pretty careful about not keeping interrupts disabled for anything like that long. 

Poking around on the interwebs I found [this](https://forums.raspberrypi.com/viewtopic.php?p=2142153&hilit=repeating_timer_callback#p2142153) which hints at odd behavior "When I add the timer with add_repeating_timer_us() to the default alarm pool, the callback isn't called anymore after some while. (Minutes to hours, depends on config.) When I create a separate alarm pool, everything is fine." Not my behavior exactly, but maybe look into "creating a separate pool?"

That was easy enough. Now we'll see if the watchdog barks. Well, some days later I got:

```
Watchdog barked: Late to callback. Motor was 0, microsNextStep was 1874762412, steps to go was 0, curMicros was 1943459845, lastCurMicros was 1943420031.
```

So, even with its own alarm pool, the callback got invoked late after something like 3 days. 39122 μs late. About the same as with the default pool. Huh. I wonder what the circumstances could be. 

I'd guess it comes from some thread turning off interrupts for a longish stretch. 39-40 ms is a pretty long stretch, though. I don't think it's me since the error occurred when the steps-to-go for the ls motor was 0. That means the callback was doing almost nothing and, since nothing else interesting was going on, only the same old same old was going on in my code. No interrupts being disabled. I'd guess its in either WiFi or NTP.

Does it really matter all that much to me in this application? I'm guessing not. I'll check around to see if it's been reported, but in the meantime, let's ignore this sort of glitch for now.

Hmmm. I don't see anything reported on [github](https://github.com/raspberrypi/pico-sdk/issues?page=1&q=is%3Aissue+callback) that looks similar to what I'm seeing. Maybe write a super simple test case to see if it turns up. Then add WiFi, and then NTP to see if I can provoke it? How much of a community helper do I feel like being? Answer: Not enough to set up a test environment in platformio that's based on the very latest SDK. Getting it wo work with Earl Pilhower's setup was tough enough.

Several days later, and after going past full moon, the watchdog is still sleeping. Several more days and still sleeping. Now more than a month later, it's still working flawlessly. Time to change the firmware to store the data persistently if the watchdog barks so that I can hang the clock up and let it run but still access debugging data if something happens.

Many months later and the watchdog never barked after loosening the criteria. So, no it doesn't really matter. Curious, though.