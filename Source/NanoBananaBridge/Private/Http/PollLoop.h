// Generic exponential-backoff HTTP GET poll loop used by FAL queue polling and
// Replicate prediction polling. Lifetime is owned by the caller's TSharedPtr.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "Containers/Ticker.h"
#include "Interfaces/IHttpRequest.h"

namespace NanoBanana::Http
{
    /** Outcome enum returned by the poll handler. */
    enum class EPollDecision : uint8
    {
        Continue,   // Not done yet — keep polling.
        Succeeded,  // Stop; FPollLoop will fire OnSucceeded with the last response body.
        Failed,     // Stop; FPollLoop will fire OnFailed.
    };

    /**
     * Polls a URL until the user-supplied DecideFn returns Succeeded/Failed
     * or MaxSeconds elapses. Backs off exponentially from InitialDelay to MaxDelay.
     */
    class FPollLoop : public TSharedFromThis<FPollLoop, ESPMode::ThreadSafe>
    {
    public:
        FPollLoop();
        ~FPollLoop();

        /** Build a request for one poll iteration (allows custom headers). */
        TFunction<TSharedRef<IHttpRequest, ESPMode::ThreadSafe>(void)> RequestFactory;

        /** Inspect a response body + status code; decide whether to continue or stop. */
        TFunction<EPollDecision(int32 /*HttpStatus*/, const FString& /*Body*/, FString& /*OutError*/)> DecideFn;

        TFunction<void(const FString& /*FinalBody*/)> OnSucceeded;
        TFunction<void(const FString& /*Error*/)> OnFailed;
        TFunction<void(float /*FractionElapsed*/)> OnProgress;

        float InitialDelaySeconds = 1.0f;
        float MaxDelaySeconds = 5.0f;
        float MaxTotalSeconds = 120.0f;
        float BackoffMultiplier = 1.5f;

        void Start();
        void Cancel();

    private:
        void ScheduleNext();
        bool TickPoll(float Dt);
        void IssueRequest();

        FTSTicker::FDelegateHandle TickerHandle;
        TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> InFlight;

        double StartTime = 0.0;
        float NextDelay = 1.0f;
        bool bCanceled = false;
        bool bDone = false;
    };
}
