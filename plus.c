/*
 * plus.c — paginator plicarum
 *
 * Instrumentum ad plicas longas paginatim inspiciendas,
 * simile "more" et "less" sed simplicius et modernius.
 * Alveum alterum terminalis adhibet, fenestram ad
 * redimensionem accommodat, et quaesita lumine notat.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

/* ================================================================
 * constantes
 * ================================================================ */

#define CAPACITAS_INITIALIS  4096
#define LIM_LINEAE           4194304
#define LIM_LONGITUDO        65536
#define LIM_QUAESITUM        256
#define LATITUDO_TABULAE     8
#define LIM_ALVEUS           (1 << 20)

/* claves speciales — supra intervallum ASCII */

enum {
    CLAVIS_SURSUM         = 256,
    CLAVIS_DEORSUM        = 257,
    CLAVIS_PAGINA_SURSUM  = 258,
    CLAVIS_PAGINA_DEORSUM = 259,
    CLAVIS_DOMUS          = 260,
    CLAVIS_FINIS          = 261,
};

/* ================================================================
 * typi
 * ================================================================ */

typedef struct {
    char **lineae;
    int    num_linearum;
    int    capacitas;
} textus_t;

typedef struct {
    textus_t        textus;
    const char     *nomen_plicae;
    struct termios  pristinus;
    char            quaesitum[LIM_QUAESITUM];
    char            nuntius[256];
    int             latitudo;
    int             altitudo;
    int             positio;
    int             crudus;
    int             fd_tty;
} status_t;

/* ================================================================
 * status globalis
 * ================================================================ */

static status_t S;
static volatile sig_atomic_t sig_redimensio = 0;

static char alveus_dep[LIM_ALVEUS];
static int  alveus_lon = 0;

/* ================================================================
 * alveus depictionis — collectio ante emissionem
 * ================================================================ */

static void alv_purga(void)
{
    alveus_lon = 0;
}

static void alv_adde(const char *s, int n)
{
    if (alveus_lon + n > LIM_ALVEUS)
        n = LIM_ALVEUS - alveus_lon;
    memcpy(alveus_dep + alveus_lon, s, n);
    alveus_lon += n;
}

static void alv_chorda(const char *s)
{
    alv_adde(s, (int)strlen(s));
}

static void alv_emitte(void)
{
    int pos = 0;
    while (pos < alveus_lon) {
        ssize_t n = write(STDOUT_FILENO, alveus_dep + pos, alveus_lon - pos);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        pos += n;
    }
    alv_purga();
}

/* ================================================================
 * terminalis — administratio
 * ================================================================ */

static void dimensio_lege(void)
{
    struct winsize ws;
    if (
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0
        && ws.ws_col > 0 && ws.ws_row > 0
    ) {
        S.latitudo = ws.ws_col;
        S.altitudo = ws.ws_row;
    } else {
        S.latitudo = 80;
        S.altitudo = 24;
    }
}

static void terminalis_crudus(void)
{
    if (S.crudus)
        return;
    if (tcgetattr(S.fd_tty, &S.pristinus) < 0)
        return;

    struct termios t = S.pristinus;
    t.c_lflag       &= ~(ECHO | ICANON | ISIG | IEXTEN);
    t.c_iflag       &= ~(IXON | ICRNL);
    t.c_cc[VMIN]  = 1;
    t.c_cc[VTIME] = 0;

    if (tcsetattr(S.fd_tty, TCSAFLUSH, &t) == 0)
        S.crudus = 1;
}

static void terminalis_restitue(void)
{
    if (!S.crudus)
        return;
    tcsetattr(S.fd_tty, TCSAFLUSH, &S.pristinus);
    S.crudus = 0;
}

static void alveus_alter_intra(void)
{
    write(STDOUT_FILENO, "\033[?1049h", 8);
}

static void alveus_alter_exi(void)
{
    write(STDOUT_FILENO, "\033[?1049l", 8);
}

static void cursor_cela(void)
{
    write(STDOUT_FILENO, "\033[?25l", 6);
}

static void cursor_ostende(void)
{
    write(STDOUT_FILENO, "\033[?25h", 6);
}

/* ================================================================
 * signa
 * ================================================================ */

static void tractator_redimensio(int sig)
{
    (void)sig;
    sig_redimensio = 1;
}

static void tractator_exi(int sig)
{
    (void)sig;
    cursor_ostende();
    alveus_alter_exi();
    terminalis_restitue();
    _exit(0);
}

static void signa_para(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = tractator_redimensio;
    sa.sa_flags   = 0;
    sigaction(SIGWINCH, &sa, NULL);

    sa.sa_handler = tractator_exi;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* ================================================================
 * textus — lectio plicae in memoriam
 * ================================================================ */

static void textus_initia(textus_t *t)
{
    t->capacitas    = CAPACITAS_INITIALIS;
    t->num_linearum = 0;
    t->lineae       = malloc(sizeof(char *) * t->capacitas);
    if (!t->lineae) {
        fprintf(stderr, "plus: memoria exhausta\n");
        exit(1);
    }
}

static void textus_adde_lineam(textus_t *t, const char *linea, int lon)
{
    if (t->num_linearum >= LIM_LINEAE)
        return;
    if (t->num_linearum >= t->capacitas) {
        t->capacitas *= 2;
        char **nova = realloc(t->lineae, sizeof(char *) * t->capacitas);
        if (!nova) {
            fprintf(stderr, "plus: memoria exhausta\n");
            exit(1);
        }
        t->lineae = nova;
    }
    char *copia = malloc(lon + 1);
    if (!copia) {
        fprintf(stderr, "plus: memoria exhausta\n");
        exit(1);
    }
    memcpy(copia, linea, lon);
    copia[lon] = '\0';
    t->lineae[t->num_linearum++] = copia;
}

static int textus_lege(textus_t *t, FILE *fons)
{
    textus_initia(t);
    char alveus[LIM_LONGITUDO];

    while (fgets(alveus, sizeof(alveus), fons)) {
        int lon = (int)strlen(alveus);
        while (
            lon > 0
            && (alveus[lon - 1] == '\n' || alveus[lon - 1] == '\r')
        )
            lon--;
        textus_adde_lineam(t, alveus, lon);
    }
    return t->num_linearum;
}

static void textus_libera(textus_t *t)
{
    for (int i = 0; i < t->num_linearum; i++)
        free(t->lineae[i]);
    free(t->lineae);
    t->lineae       = NULL;
    t->num_linearum = 0;
    t->capacitas    = 0;
}

/* ================================================================
 * quaesitum — investigatio in textu
 * ================================================================ */

/*
 * quaere_in_chorda — quaerit acum in foeno sine discrimine
 * magnitudinis litterarum. reddit indicem primum vel NULL.
 */
static const char *quaere_in_chorda(const char *foenum, const char *acus)
{
    int lon = (int)strlen(acus);
    if (lon == 0)
        return NULL;

    for (const char *p = foenum; *p; p++) {
        int par = 1;
        for (int i = 0; i < lon; i++) {
            if (
                !p[i]
                || tolower((unsigned char)p[i])
                != tolower((unsigned char)acus[i])
            ) {
                par = 0;
                break;
            }
        }
        if (par)
            return p;
    }
    return NULL;
}

static int quaere_proximum(int a_linea)
{
    if (!S.quaesitum[0])
        return 0;

    for (int i = a_linea; i < S.textus.num_linearum; i++) {
        if (quaere_in_chorda(S.textus.lineae[i], S.quaesitum)) {
            S.positio = i;
            return 1;
        }
    }
    /* circumvolve ad initium */
    for (int i = 0; i < a_linea && i < S.textus.num_linearum; i++) {
        if (quaere_in_chorda(S.textus.lineae[i], S.quaesitum)) {
            S.positio = i;
            snprintf(
                S.nuntius, sizeof(S.nuntius), "(circumvolutum)"
            );
            return 1;
        }
    }
    snprintf(S.nuntius, sizeof(S.nuntius), "non inventum: %s", S.quaesitum);
    return 0;
}

static int quaere_priorem(int a_linea)
{
    if (!S.quaesitum[0])
        return 0;

    for (int i = a_linea; i >= 0; i--) {
        if (quaere_in_chorda(S.textus.lineae[i], S.quaesitum)) {
            S.positio = i;
            return 1;
        }
    }
    /* circumvolve ad finem */
    for (int i = S.textus.num_linearum - 1; i > a_linea; i--) {
        if (quaere_in_chorda(S.textus.lineae[i], S.quaesitum)) {
            S.positio = i;
            snprintf(
                S.nuntius, sizeof(S.nuntius), "(circumvolutum)"
            );
            return 1;
        }
    }
    snprintf(S.nuntius, sizeof(S.nuntius), "non inventum: %s", S.quaesitum);
    return 0;
}

/* ================================================================
 * clavis — lectio clavium
 * ================================================================ */

static int octettum_paratum(int ms)
{
    struct pollfd pfd = { .fd = S.fd_tty, .events = POLLIN };
    return poll(&pfd, 1, ms) > 0;
}

static int clavis_lege(void)
{
    unsigned char c;
    if (read(S.fd_tty, &c, 1) != 1)
        return -1;
    if (c != 27)
        return c;

    /* ESC — fortasse initium sequentiae */
    if (!octettum_paratum(50))
        return 27;

    unsigned char seq[3];
    if (read(S.fd_tty, &seq[0], 1) != 1)
        return 27;
    if (seq[0] != '[')
        return 27;
    if (read(S.fd_tty, &seq[1], 1) != 1)
        return 27;

    /* ESC [ n ~ */
    if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(S.fd_tty, &seq[2], 1) != 1)
            return 27;
        if (seq[2] == '~') {
            switch (seq[1]) {
            case '1': return CLAVIS_DOMUS;
            case '4': return CLAVIS_FINIS;
            case '5': return CLAVIS_PAGINA_SURSUM;
            case '6': return CLAVIS_PAGINA_DEORSUM;
            }
        }
        return 27;
    }

    switch (seq[1]) {
    case 'A': return CLAVIS_SURSUM;
    case 'B': return CLAVIS_DEORSUM;
    case 'H': return CLAVIS_DOMUS;
    case 'F': return CLAVIS_FINIS;
    }
    return 27;
}

/* ================================================================
 * depictio
 * ================================================================ */

static void depinge_lineam(const char *linea, int latitudo)
{
    int lon_q = S.quaesitum[0] ? (int)strlen(S.quaesitum) : 0;

    /* inveni locos quaesiti in hac linea */
    int loci[128];
    int num_loc = 0;
    if (lon_q > 0) {
        const char *p = linea;
        while (num_loc < 128) {
            p = quaere_in_chorda(p, S.quaesitum);
            if (!p)
                break;
            loci[num_loc++] = (int)(p - linea);
            p++;
        }
    }

    int col     = 0;
    int idx_loc = 0;
    int in_luce = 0;

    for (int pos = 0; linea[pos] && col < latitudo; pos++) {
        /* intra lucem si ad locum quaesiti pervenimus */
        if (idx_loc < num_loc && pos == loci[idx_loc]) {
            if (!in_luce)
                alv_chorda("\033[43;30m");
            in_luce = lon_q;
            idx_loc++;
            while (idx_loc < num_loc && loci[idx_loc] < pos + lon_q)
                idx_loc++;
        }

        if (linea[pos] == '\t') {
            int spatia = LATITUDO_TABULAE - (col % LATITUDO_TABULAE);
            for (int s = 0; s < spatia && col < latitudo; s++) {
                alv_adde(" ", 1);
                col++;
            }
        } else if ((unsigned char)linea[pos] < 32) {
            if (col + 2 > latitudo)
                break;
            char repr[2] = { '^', '@' + linea[pos] };
            alv_adde(repr, 2);
            col += 2;
        } else {
            alv_adde(linea + pos, 1);
            col++;
        }

        if (in_luce > 0 && --in_luce == 0)
            alv_chorda("\033[0m");
    }

    if (in_luce > 0)
        alv_chorda("\033[0m");
}

static void depinge_statum(void)
{
    char barra[512];
    int versus  = S.altitudo - 1;
    int ultimus = S.positio + versus;
    if (ultimus > S.textus.num_linearum)
        ultimus = S.textus.num_linearum;

    const char *nomen = S.nomen_plicae ? S.nomen_plicae : "(stdin)";
    int lon;

    if (S.nuntius[0]) {
        lon = snprintf(
            barra, sizeof(barra), " %s  %d-%d/%d  %s",
            nomen, S.positio + 1, ultimus,
            S.textus.num_linearum, S.nuntius
        );
        S.nuntius[0] = '\0';
    } else if (S.textus.num_linearum == 0) {
        lon = snprintf(barra, sizeof(barra), " %s  (vacuus)", nomen);
    } else {
        int centesima = (ultimus * 100) / S.textus.num_linearum;
        lon = snprintf(
            barra, sizeof(barra), " %s  %d-%d/%d  %d%%",
            nomen, S.positio + 1, ultimus,
            S.textus.num_linearum, centesima
        );
    }

    if (lon < 0)
        lon = 0;
    if (lon > S.latitudo)
        lon = S.latitudo;

    alv_chorda("\033[7m");
    alv_adde(barra, lon);
    for (int i = lon; i < S.latitudo; i++)
        alv_adde(" ", 1);
    alv_chorda("\033[0m");
}

static void depinge(void)
{
    alv_purga();
    alv_chorda("\033[H");

    int versus = S.altitudo - 1;
    for (int i = 0; i < versus; i++) {
        alv_chorda("\033[K");
        int idx = S.positio + i;
        if (idx < S.textus.num_linearum)
            depinge_lineam(S.textus.lineae[idx], S.latitudo);
        else
            alv_chorda("\033[90m~\033[0m");
        alv_chorda("\r\n");
    }

    depinge_statum();
    alv_emitte();
}

/* ================================================================
 * modus quaesiti — investigatio interactiva
 * ================================================================ */

static void depinge_promptum(const char *alv, int lon)
{
    char seq[32];
    int n = snprintf(seq, sizeof(seq), "\033[%d;1H", S.altitudo);
    write(STDOUT_FILENO, seq, n);

    /* barra inversa cum "/" et textu quaesiti */
    write(STDOUT_FILENO, "\033[7m/", 5);
    int vis = lon;
    if (vis > S.latitudo - 1)
        vis = S.latitudo - 1;
    if (vis > 0)
        write(STDOUT_FILENO, alv, vis);
    for (int i = vis + 1; i < S.latitudo; i++)
        write(STDOUT_FILENO, " ", 1);
    write(STDOUT_FILENO, "\033[0m", 4);

    n = snprintf(seq, sizeof(seq), "\033[%d;%dH", S.altitudo, 2 + vis);
    write(STDOUT_FILENO, seq, n);
}

static void modus_quaesiti(void)
{
    char alv[LIM_QUAESITUM];
    int lon = 0;

    cursor_ostende();
    depinge_promptum(alv, 0);

    for (;;) {
        unsigned char c;
        if (read(S.fd_tty, &c, 1) != 1)
            break;

        if (c == '\r' || c == '\n') {
            alv[lon] = '\0';
            if (lon > 0) {
                strncpy(S.quaesitum, alv, LIM_QUAESITUM - 1);
                S.quaesitum[LIM_QUAESITUM - 1] = '\0';
                quaere_proximum(S.positio);
            }
            break;
        }
        if (c == 27 || c == 3)
            break;

        if (c == 127 || c == 8) {
            if (lon > 0)
                lon--;
        } else if (
            c >= 32 && lon < LIM_QUAESITUM - 1
            && lon < S.latitudo - 2
        ) {
            alv[lon++] = c;
        }
        depinge_promptum(alv, lon);
    }

    cursor_cela();
}

/* ================================================================
 * editor — lanciatio externa
 * ================================================================ */

static void lancia_editorem(void)
{
    if (!S.nomen_plicae) {
        snprintf(
            S.nuntius, sizeof(S.nuntius),
            "editor: nullum nomen plicae (ex stdin)"
        );
        return;
    }

    const char *editor = getenv("EDITOR");
    if (!editor || !*editor)
        editor = "vi";

    char lineam[32];
    snprintf(lineam, sizeof(lineam), "+%d", S.positio + 1);

    /* restitue terminalem antequam editor lanciatur */
    cursor_ostende();
    alveus_alter_exi();
    terminalis_restitue();

    /* ignora signa interruptionis dum editor currit */
    struct sigaction sa_ign, sa_olim_int, sa_olim_term;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sigaction(SIGINT,  &sa_ign, &sa_olim_int);
    sigaction(SIGTERM, &sa_ign, &sa_olim_term);

    pid_t pid = fork();
    if (pid < 0) {
        snprintf(
            S.nuntius, sizeof(S.nuntius),
            "fork: %s", strerror(errno)
        );
    } else if (pid == 0) {
        /* in filio: restitue signa pristina */
        struct sigaction sa_def;
        memset(&sa_def, 0, sizeof(sa_def));
        sa_def.sa_handler = SIG_DFL;
        sigemptyset(&sa_def.sa_mask);
        sigaction(SIGINT,  &sa_def, NULL);
        sigaction(SIGTERM, &sa_def, NULL);

        char *args[] = {
            (char *)editor,
            lineam,
            (char *)S.nomen_plicae,
            NULL
        };
        execvp(editor, args);
        _exit(127);
    } else {
        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            (void)0;
    }

    /* redintegra signa */
    sigaction(SIGINT,  &sa_olim_int,  NULL);
    sigaction(SIGTERM, &sa_olim_term, NULL);

    /* redintegra statum terminalis */
    terminalis_crudus();
    alveus_alter_intra();
    cursor_cela();
    dimensio_lege();

    /* relege plicam — potest mutata esse */
    FILE *fons = fopen(S.nomen_plicae, "r");
    if (fons) {
        textus_libera(&S.textus);
        textus_lege(&S.textus, fons);
        fclose(fons);
    } else {
        snprintf(
            S.nuntius, sizeof(S.nuntius),
            "relectio: %s", strerror(errno)
        );
    }
}

/* ================================================================
 * positio — cohibitio liminum
 * ================================================================ */

static void positio_coerce(void)
{
    int max = S.textus.num_linearum - (S.altitudo - 1);
    if (max < 0)
        max = 0;
    if (S.positio > max)
        S.positio = max;
    if (S.positio < 0)
        S.positio = 0;
}

/* ================================================================
 * principale
 * ================================================================ */

int main(int argc, char **argv)
{
    memset(&S, 0, sizeof(S));
    S.fd_tty = -1;

    if (
        argc > 1
        && (
            strcmp(argv[1], "-h") == 0
            || strcmp(argv[1], "--auxilium") == 0
        )
    ) {
        fprintf(
            stderr,
            "usus: plus [plica]\n"
            "\n"
            "claves:\n"
            "  j/deorsum/intra    lineam deorsum\n"
            "  k/sursum           lineam sursum\n"
            "  spatium/f          paginam deorsum\n"
            "  b                  paginam sursum\n"
            "  d                  dimidiam paginam deorsum\n"
            "  u                  dimidiam paginam sursum\n"
            "  g                  ad initium\n"
            "  G                  ad finem\n"
            "  /                  quaere\n"
            "  n                  proximum inventum\n"
            "  N                  prius inventum\n"
            "  v                  lancia editorem ($EDITOR)\n"
            "  q                  exi\n"
        );
        return 0;
    }

    /* aperi fontem */
    FILE *fons     = NULL;
    int   ex_stdin = 0;

    if (argc > 1) {
        S.nomen_plicae = argv[1];
        fons = fopen(argv[1], "r");
        if (!fons) {
            fprintf(stderr, "plus: '%s': %s\n", argv[1], strerror(errno));
            return 1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        fons     = stdin;
        ex_stdin = 1;
    } else {
        fprintf(stderr, "usus: plus [plica]\n");
        return 1;
    }

    textus_lege(&S.textus, fons);
    if (!ex_stdin && fons)
        fclose(fons);

    /* si stdout non terminalis, transscribe recte */
    if (!isatty(STDOUT_FILENO)) {
        for (int i = 0; i < S.textus.num_linearum; i++)
            puts(S.textus.lineae[i]);
        textus_libera(&S.textus);
        return 0;
    }

    dimensio_lege();

    /* si textus in una pagina capit, ostende sine paginatione */
    if (S.textus.num_linearum <= S.altitudo - 1) {
        for (int i = 0; i < S.textus.num_linearum; i++)
            puts(S.textus.lineae[i]);
        textus_libera(&S.textus);
        return 0;
    }

    /* para terminalis pro modo interactivo */
    if (ex_stdin) {
        S.fd_tty = open("/dev/tty", O_RDONLY);
        if (S.fd_tty < 0) {
            fprintf(stderr, "plus: /dev/tty: %s\n", strerror(errno));
            return 1;
        }
    } else {
        S.fd_tty = STDIN_FILENO;
    }

    signa_para();
    terminalis_crudus();
    alveus_alter_intra();
    cursor_cela();
    depinge();

    /* ansa principalis */
    for (;;) {
        int c = clavis_lege();

        if (sig_redimensio) {
            sig_redimensio = 0;
            dimensio_lege();
            positio_coerce();
            depinge();
        }

        if (c < 0)
            continue;

        int mutatum = 0;

        switch (c) {
        case 'q':
        case 'Q':
        case 3: /* ^C */
            goto finis;

        case 'j':
        case CLAVIS_DEORSUM:
        case '\r':
            S.positio++;
            mutatum = 1;
            break;

        case 'k':
        case CLAVIS_SURSUM:
            S.positio--;
            mutatum = 1;
            break;

        case ' ':
        case 'f':
        case CLAVIS_PAGINA_DEORSUM:
            S.positio += S.altitudo - 2;
            mutatum = 1;
            break;

        case 'b':
        case CLAVIS_PAGINA_SURSUM:
            S.positio -= S.altitudo - 2;
            mutatum = 1;
            break;

        case 'd':
            S.positio += (S.altitudo - 1) / 2;
            mutatum = 1;
            break;

        case 'u':
            S.positio -= (S.altitudo - 1) / 2;
            mutatum = 1;
            break;

        case 'g':
        case CLAVIS_DOMUS:
            S.positio = 0;
            mutatum   = 1;
            break;

        case 'G':
        case CLAVIS_FINIS:
            S.positio = S.textus.num_linearum;
            mutatum   = 1;
            break;

        case '/':
            modus_quaesiti();
            mutatum = 1;
            break;

        case 'n':
            quaere_proximum(S.positio + 1);
            mutatum = 1;
            break;

        case 'N':
            quaere_priorem(S.positio - 1);
            mutatum = 1;
            break;

        case 'v':
            lancia_editorem();
            positio_coerce();
            mutatum = 1;
            break;
        }

        if (mutatum) {
            positio_coerce();
            depinge();
        }
    }

finis:
    cursor_ostende();
    alveus_alter_exi();
    terminalis_restitue();
    textus_libera(&S.textus);
    if (S.fd_tty >= 0 && S.fd_tty != STDIN_FILENO)
        close(S.fd_tty);
    return 0;
}
