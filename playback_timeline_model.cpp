#include "playback_timeline_model.h"
#include <algorithm>

static inline TimelineSpan clip(const TimelineSpan& s, qint64 a, qint64 b) {
    TimelineSpan c{ std::max(s.start_ns, a), std::min(s.end_ns, b) };
    if (c.end_ns < c.start_ns) c.end_ns = c.start_ns;
    return c;
}

void PlaybackTimelineModel::build(qint64 dayStartNs, qint64 dayEndNs,
                                  const QVector<TimelineSpan>& raw) {
    t0_ = dayStartNs; t1_ = dayEndNs; spans_.clear();
    QVector<TimelineSpan> v; v.reserve(raw.size());
    for (const auto& s : raw) {
        if (s.end_ns <= dayStartNs || s.start_ns >= dayEndNs) continue;
        v.push_back(clip(s, dayStartNs, dayEndNs));
    }
    std::sort(v.begin(), v.end(), [](auto& a, auto& b){ return a.start_ns < b.start_ns; });
    // merge overlaps/touches
    for (const auto& s : v) {
        if (spans_.isEmpty() || s.start_ns > spans_.last().end_ns) {
            spans_.push_back(s);
        } else {
            spans_.last().end_ns = std::max(spans_.last().end_ns, s.end_ns);
        }
    }
}

qreal PlaybackTimelineModel::fractionFor(qint64 t_ns) const {
    if (t1_ <= t0_) return 0.0;
    if (t_ns <= t0_) return 0.0;
    if (t_ns >= t1_) return 1.0;
    return qreal(t_ns - t0_) / qreal(t1_ - t0_);
}

qint64 PlaybackTimelineModel::totalCoveredNs() const {
    qint64 sum = 0;
    for (const auto& s : spans_) sum += std::max<qint64>(0, s.end_ns - s.start_ns);
    return sum;
}
