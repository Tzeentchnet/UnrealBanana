#include "PollLoop.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

namespace NanoBanana::Http
{
    FPollLoop::FPollLoop() = default;

    FPollLoop::~FPollLoop()
    {
        Cancel();
    }

    void FPollLoop::Start()
    {
        StartTime = FPlatformTime::Seconds();
        NextDelay = FMath::Max(0.1f, InitialDelaySeconds);
        // Issue first request immediately, then back off between subsequent polls.
        IssueRequest();
    }

    void FPollLoop::Cancel()
    {
        bCanceled = true;
        bDone = true;
        if (TickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            TickerHandle.Reset();
        }
        if (InFlight.IsValid())
        {
            InFlight->CancelRequest();
            InFlight.Reset();
        }
    }

    void FPollLoop::ScheduleNext()
    {
        if (bDone || bCanceled) return;

        const double Elapsed = FPlatformTime::Seconds() - StartTime;
        if (Elapsed >= MaxTotalSeconds)
        {
            bDone = true;
            if (OnFailed) OnFailed(FString::Printf(TEXT("Poll loop timed out after %.1f seconds"), (float)Elapsed));
            return;
        }

        if (OnProgress)
        {
            OnProgress(FMath::Clamp((float)(Elapsed / FMath::Max(1.0f, MaxTotalSeconds)), 0.0f, 1.0f));
        }

        TWeakPtr<FPollLoop, ESPMode::ThreadSafe> WeakThis = AsShared();
        const float Delay = NextDelay;
        NextDelay = FMath::Min(MaxDelaySeconds, NextDelay * BackoffMultiplier);

        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [WeakThis](float /*Dt*/) -> bool
            {
                if (TSharedPtr<FPollLoop, ESPMode::ThreadSafe> Pinned = WeakThis.Pin())
                {
                    Pinned->TickerHandle.Reset();
                    Pinned->IssueRequest();
                }
                return false; // one-shot
            }), Delay);
    }

    void FPollLoop::IssueRequest()
    {
        if (bDone || bCanceled || !RequestFactory) return;

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = RequestFactory();
        InFlight = Req;

        TWeakPtr<FPollLoop, ESPMode::ThreadSafe> WeakThis = AsShared();
        Req->OnProcessRequestComplete().BindLambda(
            [WeakThis](FHttpRequestPtr /*ReqPtr*/, FHttpResponsePtr Resp, bool bSucceeded)
            {
                TSharedPtr<FPollLoop, ESPMode::ThreadSafe> Pinned = WeakThis.Pin();
                if (!Pinned.IsValid() || Pinned->bDone || Pinned->bCanceled) return;

                Pinned->InFlight.Reset();

                if (!bSucceeded || !Resp.IsValid())
                {
                    Pinned->bDone = true;
                    if (Pinned->OnFailed) Pinned->OnFailed(TEXT("Poll request failed (network)."));
                    return;
                }

                const int32 Code = Resp->GetResponseCode();
                const FString Body = Resp->GetContentAsString();

                FString Err;
                EPollDecision Decision = EPollDecision::Continue;
                if (Pinned->DecideFn)
                {
                    Decision = Pinned->DecideFn(Code, Body, Err);
                }

                switch (Decision)
                {
                case EPollDecision::Succeeded:
                    Pinned->bDone = true;
                    if (Pinned->OnSucceeded) Pinned->OnSucceeded(Body);
                    break;
                case EPollDecision::Failed:
                    Pinned->bDone = true;
                    if (Pinned->OnFailed) Pinned->OnFailed(Err.IsEmpty() ? FString::Printf(TEXT("Poll failed (HTTP %d)"), Code) : Err);
                    break;
                case EPollDecision::Continue:
                default:
                    Pinned->ScheduleNext();
                    break;
                }
            });

        Req->ProcessRequest();
    }

    bool FPollLoop::TickPoll(float /*Dt*/)
    {
        return false;
    }
}
