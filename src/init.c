#include <assert.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <asm/types.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/socket.h>

static_assert(NLMSG_HDRLEN == sizeof(struct nlmsghdr),
              "make sure we don't need any extra alignment (nlmsghdr)");
static_assert(GENL_HDRLEN == sizeof(struct genlmsghdr),
              "make sure we don't need any extra alignment (genlmsghdr)");
static_assert(NLA_HDRLEN == sizeof(struct nlattr),
              "make sure we don't need any extra alignment (nlattr)");

struct state {
    int fd;
    uint16_t family;
    uint32_t mcast_group;
};

// drivers/acpi/event.c:78
// https://github.com/torvalds/linux/blob/788a024921c48985939f8241c1ff862a7374d8f9/drivers/acpi/event.c#L78
#define ACPI_GENL_FAMILY_NAME "acpi_event"
#define ACPI_GENL_FAMILY_NAME_LEN 10
#define ACPI_GENL_VERSION 0x01
#define ACPI_GENL_MCAST_GROUP_NAM "acpi_mc_group"
#define ACPI_GENL_MCAST_GROUP_NAM_LEN 13

// drivers/acpi/event.c:73
// https://github.com/torvalds/linux/blob/788a024921c48985939f8241c1ff862a7374d8f9/drivers/acpi/event.c#L73
#define ACPI_GENL_CMD_EVENT 1

// drivers/acpi/event.c:65
// https://github.com/torvalds/linux/blob/788a024921c48985939f8241c1ff862a7374d8f9/drivers/acpi/event.c#L65
#define ACPI_GENL_ATTR_EVENT 1

#define LENGTH(xs) (sizeof(xs)/sizeof((xs)[0]))

const char* interpret_netlink_flag(uint32_t flag)
{
    switch(flag) {
    case 0: return "";
    case 0x100: return "NLM_F_ROOT";
    case NLM_F_ACK: return "ACK";
    default:
        err(3, "not defined flag: %"PRIu32, flag);
    }
}

static const char* interpret_netlink_flag_ack(uint32_t flag)
{
    switch(flag) {
    case 0: return "";
    case NLM_F_CAPPED: return "NLM_F_CAPPED";
    default:
        err(3, "not defined flag: %"PRIu32, flag);
    }
}

static const char* interpret_genetlink_cmd(uint8_t cmd)
{
    switch(cmd) {
    case CTRL_CMD_NEWFAMILY: return "CTRL_CMD_NEWFAMILY";
    case CTRL_CMD_GETFAMILY: return "CTRL_CMD_GETFAMILY";
    default:
        err(3, "not defined cmd: %"PRIu8, cmd);
    }
}

static void setup_netlink(struct state* st, int flags)
{
    st->fd = socket(AF_NETLINK, SOCK_RAW | flags, NETLINK_GENERIC);
    if(st->fd < 0) perror("socket()"), exit(1);

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK, 0 };

    int r = bind(st->fd, (struct sockaddr*)&sa, sizeof(sa));
    if(r != 0) perror("bind()"), exit(1);
}

struct outbound_msg {
    uint16_t type; uint32_t flags;
    void* buf; size_t len;
    uint32_t seq;

    struct outbound_msg* next;
};

static struct outbound_msg* outbound_queue;

static uint32_t enqueue_outbound(uint16_t type, uint32_t flags,
                                 const void* buf, size_t len)
{
    static uint32_t seq;

    struct outbound_msg* m = calloc(sizeof(*m), 1);
    if(!m) err(1, "calloc");

    m->type = type; m->flags = flags; m->len = len;
    m->buf = malloc(len); if(!m->buf) err(1, "malloc");
    m->seq = seq++;
    memcpy(m->buf, buf, len);

    // append
    struct outbound_msg** p = &outbound_queue;
    while(*p != NULL) p = &(*p)->next;
    *p = m;

    printf("enqueued: seq=%"PRIu32"\n", m->seq);
    return m->seq;
}

static void dequeue_outbound(void)
{
    if(!outbound_queue) err(1, "enqueue_outbound on empty queue");

    struct outbound_msg* m = outbound_queue;
    outbound_queue = m->next;

    free(m->buf); free(m);
}

static ssize_t send_netlink(int fd, const struct outbound_msg* m)
{

    size_t msg_len = sizeof(struct nlmsghdr) + m->len;
    if(msg_len > UINT32_MAX || m->len >= UINT32_MAX - sizeof(struct nlmsghdr))
        err(1, "buffer too big to send");

    struct nlmsghdr nhd = {
        .nlmsg_len = msg_len,
        .nlmsg_type = m->type,
        .nlmsg_flags = m->flags,
        .nlmsg_seq = m->seq,
        .nlmsg_pid = 0,
    };

    struct iovec vs[] = {
        { .iov_base = &nhd, .iov_len = sizeof(nhd) },
        { .iov_base = m->buf, .iov_len = m->len },
    };

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK, 0 };

    struct msghdr mhd = {
        .msg_name = &sa, .msg_namelen = sizeof(sa),
        .msg_iov = vs, .msg_iovlen = LENGTH(vs),
    };

    ssize_t r = sendmsg(fd, &mhd, 0);
    if(r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        perror("sendto"), exit(1);

    printf("sent: seq=%"PRIu32" type=%"PRIu16"\n", m->seq, m->type);

    return r;
}

static void handle_mcast_group(struct state* st,
                               const char* name, uint32_t gid)
{
    if(strncmp(name, ACPI_GENL_MCAST_GROUP_NAM,
               ACPI_GENL_MCAST_GROUP_NAM_LEN) == 0) {
        st->mcast_group = gid;

        int r = setsockopt(st->fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
                           &st->mcast_group, sizeof(st->mcast_group));
        if(r != 0) perror("setsockopt()"), exit(1);
        printf("joined mcast group %s\n", name);
    }
}

static void parse_new_family_payload(struct state* st,
                                     const void* buf, size_t len)
{
    while(len > 0) {
        struct nlattr* a = (struct nlattr*)buf;

        if(a->nla_type == CTRL_ATTR_FAMILY_ID) {
            st->family = *(uint16_t*)(buf + NLA_HDRLEN);
            printf("family id=%"PRIu16"\n", st->family);
        }

        if(a->nla_type == CTRL_ATTR_MCAST_GROUPS) {
            size_t l = NLA_ALIGN(a->nla_len) - NLA_HDRLEN;
            const void *b = buf + NLA_HDRLEN;

            while(l > 0 && l <= len - NLA_HDRLEN) {
                struct nlattr* g = (struct nlattr*)b;
                size_t j = NLA_ALIGN(g->nla_len) - NLA_HDRLEN;
                const void *c = b + NLA_HDRLEN;

                int64_t gid = -1;
                const char* name = NULL;

                while(j > 0 && j <= l - NLA_HDRLEN) {
                    struct nlattr* ga = (struct nlattr*)c;

                    switch(ga->nla_type) {
                    case CTRL_ATTR_MCAST_GRP_ID:
                        gid = *(uint32_t*)(c + NLA_HDRLEN); break;
                    case CTRL_ATTR_MCAST_GRP_NAME:
                        name = c + NLA_HDRLEN; break;
                    default: err(1, "unexpected attr type in a mcast group");
                    }

                    c += NLA_ALIGN(ga->nla_len);
                    j -= NLA_ALIGN(ga->nla_len);
                }

                if(gid == -1 || name == NULL) err(1, "malformed mcast group");

                handle_mcast_group(st, name, gid);

                b += NLA_ALIGN(g->nla_len);
                l -= NLA_ALIGN(g->nla_len);
            }
        }

        buf += NLA_ALIGN(a->nla_len);
        len -= NLA_ALIGN(a->nla_len);
    }
}

static void parse_acpi_payload(struct state* st, const void* buf, size_t len) {
    struct genlmsghdr* g = (struct genlmsghdr*)buf;
    buf += sizeof(*g); len -= sizeof(*g);

    if(g->cmd != ACPI_GENL_CMD_EVENT)
        err(1, "unexpected command from ACPI: %"PRIu8, g->cmd);

    if(g->version != ACPI_GENL_VERSION)
        err(1, "unexpected version from ACPI: %"PRIu8, g->version);

    struct nlattr* a = (struct nlattr*)buf;

    if(a->nla_type != ACPI_GENL_ATTR_EVENT)
        err(1, "unexpected attribute from ACPI: %"PRIu8, a->nla_type);

    len = a->nla_len - NLA_HDRLEN;

    // drivers/acpi/event.c:55
    // https://github.com/torvalds/linux/blob/788a024921c48985939f8241c1ff862a7374d8f9/drivers/acpi/event.c#L55
    const struct {
        // include/acpi/acpi_bus.h:222
        // https://github.com/torvalds/linux/blob/788a024921c48985939f8241c1ff862a7374d8f9/include/acpi/acpi_bus.h#L222
        char device_class[20];
        char bus_id[15];
        uint32_t type;
        uint32_t data;
    }* event = buf + NLA_HDRLEN;

    printf("device_class=%s\n", event->device_class);
    printf("bus_id=%s\n", event->bus_id);
    printf("type=%"PRIu32"\n", event->type);
    printf("data=%"PRIu32"\n", event->data);
}

static void handle_incoming(struct state* st, const struct nlmsghdr* hd,
                            const void* buf, size_t len)
{
    if(hd->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr* e = (struct nlmsgerr*)buf;
        if(e->error == 0) {
            printf("success: msg.seq=%d seq=%d flags=%s\n",
                   e->msg.nlmsg_seq, hd->nlmsg_seq,
                   interpret_netlink_flag_ack(hd->nlmsg_flags));
        } else {
            printf("error: error=%d msg.seq=%d seq=%d\n",
                   e->error, e->msg.nlmsg_seq, hd->nlmsg_seq);
        }
    } else if(hd->nlmsg_type == GENL_ID_CTRL) {
        struct genlmsghdr* g = (struct genlmsghdr*)buf;
        buf += sizeof(*g); len -= sizeof(*g);

        if(g->cmd == CTRL_CMD_NEWFAMILY && g->version == 0x2) {
            parse_new_family_payload(st, buf, len);
        } else {
            printf("unhandled genlmsg: cmd=%s ver=%"PRIu8" seq=%d len=%zu "
                   "flags=%"PRIx32"\n",
                   interpret_genetlink_cmd(g->cmd), g->version,
                   hd->nlmsg_seq, len, hd->nlmsg_flags);
        }
    } else if(hd->nlmsg_type >= NLMSG_MIN_TYPE
              && st->family == hd->nlmsg_type) {
        printf("acpi message: seq=%"PRIu32"\n", hd->nlmsg_seq);
        parse_acpi_payload(st, buf, len);
    } else {
        printf("don't know how to handle: type=%"PRIu16" "
               "seq=%"PRIu32" flags=%"PRIx32"\n",
               hd->nlmsg_type, hd->nlmsg_seq, hd->nlmsg_flags);
    }
}

static ssize_t recv_netlink(struct state* st, void* buf, size_t len)
{
    struct sockaddr_nl sa = { 0 };
    struct nlmsghdr nhd = { 0 };

    struct iovec vs[] = {
        { .iov_base = &nhd, .iov_len = sizeof(nhd) },
        { .iov_base = buf, .iov_len = len },
    };

    struct msghdr mhd = {
        .msg_name = &sa, .msg_namelen = sizeof(sa),
        .msg_iov = vs, .msg_iovlen = LENGTH(vs),
    };

    ssize_t r = recvmsg(st->fd, &mhd, 0);

    if(r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        perror("recvmsg"), exit(1);

    if(r == 0) return 0;

    if(sa.nl_family != AF_NETLINK || mhd.msg_namelen != sizeof(sa))
        err(1, "wrong kind of message");

    if(!NLMSG_OK(&nhd, r)) err(1, "truncated message");

    handle_incoming(st, &nhd, buf, r - sizeof(nhd));

    return r - sizeof(nhd);
}

static void teardown_netlink(struct state* st)
{
    if(close(st->fd) != 0) perror("close"), exit(1);
    st->fd = -1;
}

void run_event_loop(struct state* st) {
    struct pollfd fds[] = { { .fd = st->fd, .events = POLLIN } };

    size_t timeouts = 0;

    while(1) {
        if(outbound_queue) fds[0].events |= POLLOUT;

        int r = poll(fds, LENGTH(fds), 1000);
        if(r < 0) perror("poll()"), exit(1);

        if(r == 0) {
            printf("still around... (%zu/2)\n", ++timeouts);
            if(timeouts == 2) break;
        }

        assert(fds[0].fd == st->fd);

        if(fds[0].revents & POLLIN) {
            unsigned char buf[4096];
            (void)recv_netlink(st, buf, sizeof(buf));
        }

        if(fds[0].revents & POLLOUT) {
            (void)send_netlink(st->fd, outbound_queue);
            dequeue_outbound();
            if(!outbound_queue) fds[0].events &= ~POLLOUT;
        }
    }
}

static void genl_get_family()
{
    struct {
        struct genlmsghdr ghd;
        struct nlattr a1_hd; uint32_t a1_v;
        struct nlattr a2_hd;
        char a2_v[NLA_ALIGN(ACPI_GENL_FAMILY_NAME_LEN+1)];
    } payload = {
        .ghd = {
            .cmd = CTRL_CMD_GETFAMILY,
            // net/netlink/genetlink.c:986
            // https://github.com/torvalds/linux/blob/788a024921c48985939f8241c1ff862a7374d8f9/net/netlink/genetlink.c#L986
            .version = 0x2,
            0
        },

        .a1_hd = {
            .nla_type = CTRL_ATTR_FAMILY_ID,
            .nla_len = sizeof(struct nlattr) + sizeof(uint16_t),
        },
        .a1_v = GENL_ID_CTRL,

        .a2_hd = {
            .nla_len = sizeof(struct nlattr) + ACPI_GENL_FAMILY_NAME_LEN + 1,
            .nla_type = CTRL_ATTR_FAMILY_NAME,
        },
    };
    memcpy(payload.a2_v, ACPI_GENL_FAMILY_NAME, ACPI_GENL_FAMILY_NAME_LEN+1);

    enqueue_outbound(GENL_ID_CTRL, NLM_F_REQUEST | NLM_F_ACK,
                     &payload, sizeof(payload));
}

int main(void)
{
    printf("hello\n");

    genl_get_family();

    struct state st = { 0 };

    setup_netlink(&st, SOCK_NONBLOCK | SOCK_CLOEXEC);
    run_event_loop(&st);
    teardown_netlink(&st);

    printf("good bye...\n");
    (void)reboot(LINUX_REBOOT_CMD_POWER_OFF);
    perror("reboot(.._POWER_OFF)"), exit(1);
}
