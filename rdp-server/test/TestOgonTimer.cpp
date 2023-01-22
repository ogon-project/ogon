/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Eventloop timers Test
 *
 * Copyright (c) 2020 Thincast Technologies GmbH
 *
 * Authors:
 * David Fort <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this file for any
 * purpose is hereby granted without fee, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and this
 * permission notice appear in supporting documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of this file.
 *
 * THIS FILE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "../common/global.h"
#include "../eventloop.h"
#include <winpr/sysinfo.h>

static void periodicTimerCb(void *data) {
	int *counter = (int *)data;
	*counter += 1;
}

extern "C" int TestOgonTimer(int argc, char *argv[]) {
	OGON_UNUSED(argc);
	OGON_UNUSED(argv);
	ogon_event_loop *loop;
	ogon_event_source *evsrc;
	int counter = 0;
	UINT64 endDate;

	loop = eventloop_create();
	if (!loop) return 1;

	evsrc = eventloop_add_timer(loop, 100, periodicTimerCb, &counter);
	if (!evsrc) return 2;

	// timer should be triggered and increment the counter
	if (!eventloop_dispatch_loop(loop, 200)) return 3;

	if (counter != 1) return 4;

	// timer should be triggered a second time
	if (!eventloop_dispatch_loop(loop, 200)) return 5;

	if (counter != 2) return 6;

	// timer is disabled, so nothing should happen
	eventloop_remove_source(&evsrc);
	if (eventloop_dispatch_loop(loop, 200)) return 7;

	if (counter != 2) return 8;

	/* and finally a simple measurement test
	 * program a timer every 20ms, and let it run for 1 second => should have a
	 * counter around 50
	 */
	counter = 0;
	evsrc = eventloop_add_timer(loop, 20, periodicTimerCb, &counter);
	if (!evsrc) return 9;

	endDate = GetTickCount64() + 1000;
	while (GetTickCount64() < endDate) {
		eventloop_dispatch_loop(loop, 400);
	}

	// printf("counter=%d\n", counter);
	if (counter < 48 || counter > 52) return 10;

	eventloop_remove_source(&evsrc);
	eventloop_destroy(&loop);
	return 0;
}
