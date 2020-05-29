#ifndef EXB_TIME_H
#define EXB_TIME_H
//has at least millisecond accuarcy
struct exb_timestamp {
    #ifdef EXB_TIMESTAMP_FLOAT
        double timestamp;
    #else
        long long timestamp;
    #endif
};
#define EXB_TIMESTAMP_INT_MULTIPLIER 1000000

#ifdef EXB_TIMESTAMP_FLOAT
    static struct exb_timestamp exb_timestamp(double timestamp) {
        struct exb_timestamp t = {timestamp};
        return t;
    }
    static struct exb_timestamp exb_timestamp_with_usec(long long timestamp_seconds, long long timestamp_usec) {
        struct exb_timestamp t = { (double)timestamp_seconds + ((double)timestamp_usec / 1000000.0)};
        return t;
    }
    static double exb_timestamp_get_seconds(struct exb_timestamp ts) {
        return rs.timestamp;
    }
    static struct exb_timestamp exb_timestamp_add_usec(struct exb_timestamp a, long long usec) {
        struct exb_timestamp t = {a.timestamp + usec};
        return t;
    }
    static long long exb_timestamp_get_as_usec(struct exb_timestamp ts) {
        return ts.timestamp * 1000000.0;
    }
#else
    static struct exb_timestamp exb_timestamp(long long timestamp_seconds) {
        struct exb_timestamp t = {timestamp_seconds * EXB_TIMESTAMP_INT_MULTIPLIER};
        return t;
    }
    static struct exb_timestamp exb_timestamp_with_usec(long long timestamp_seconds, long long timestamp_usec) {
        struct exb_timestamp t = {timestamp_seconds * EXB_TIMESTAMP_INT_MULTIPLIER + timestamp_usec};
        return t;
    }
    static long long exb_timestamp_get_seconds(struct exb_timestamp ts) {
        return ts.timestamp / EXB_TIMESTAMP_INT_MULTIPLIER;
    }
    static long long exb_timestamp_get_as_usec(struct exb_timestamp ts) {
        return ts.timestamp;
    }
    static struct exb_timestamp exb_timestamp_add_usec(struct exb_timestamp a, long long usec) {
        struct exb_timestamp t = {a.timestamp + usec};
        return t;
    }
#endif

static int exb_timestamp_cmp(struct exb_timestamp a, struct exb_timestamp b) {
    return (int)(a.timestamp - b.timestamp);
}

static struct exb_timestamp exb_timestamp_diff(struct exb_timestamp a, struct exb_timestamp b) {
    struct exb_timestamp t = {a.timestamp - b.timestamp};
    return t;
}

#endif