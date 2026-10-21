#include <stdio.h>
#include <time.h>
#include <dns_sd.h>

static volatile int got_result = 0;

static void cb(DNSServiceRef sdref, DNSServiceFlags flags, uint32_t ifIndex,
               DNSServiceErrorType errorCode, const char *name,
               const char *regtype, const char *domain, void *context) {
    if (!(flags & kDNSServiceFlagsAdd))
        return;                       // ignore removal events
    if (errorCode == kDNSServiceErr_NoError) {
        printf("Found: %s.%s%s\n", name, regtype, domain);
        got_result = 1;
    } else {
        fprintf(stderr, "browse error %d\n", errorCode);
        got_result = 1;               // don't hang on error
    }
    (void)sdref; (void)ifIndex; (void)context;
}

int main(void) {
    DNSServiceRef service = NULL;     // NOT a pointer you initialize to NULL
    DNSServiceErrorType err = DNSServiceBrowse(&service, 0, 0, "_ssc._tcp.",
                                               NULL, cb, NULL);
    if (err != kDNSServiceErr_NoError) {
        fprintf(stderr, "DNSServiceBrowse failed: %d\n", err);
        return 1;
    }

    // Event loop: block on the daemon socket until result or timeout
    int fd = DNSServiceRefSockFD(service);

    for (int waited = 0; waited < 5000 && !got_result; ) {  // 5s budget
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        struct timeval tv = { 1, 0 };                    // 1s slices
        int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) { perror("select"); break; }
        if (ret > 0) {
            err = DNSServiceProcessResult(service);
            if (err != kDNSServiceErr_NoError) {
                fprintf(stderr, "process result failed: %d\n", err);
                break;
            }
        } else {
            waited += 1000;
        }
    }

    if (!got_result)
        printf("Timed out waiting for _ssc._tcp services\n");

    DNSServiceRefDeallocate(service);
    return 0;
}