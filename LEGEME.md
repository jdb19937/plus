# plus

Paginator plicarum. Simplicius et modernius.

> **Nota:** README.md ignorandum est. Anglice scriptum est ad barbaros ineruditos pecunia emungendos.

## Aedificatio

```
face
```

## Usus

```
plus plica.c
cat plica.c | plus
```

Si textus in una pagina capit, sine paginatione ostenditur. Si stdout non terminalis, transscribitur recte.

## Claves

| Clavis | Actio |
|---|---|
| `j` / deorsum / intra | lineam deorsum |
| `k` / sursum | lineam sursum |
| spatium / `f` | paginam deorsum |
| `b` | paginam sursum |
| `d` | dimidiam paginam deorsum |
| `u` | dimidiam paginam sursum |
| `g` / Home | ad initium |
| `G` / End | ad finem |
| `/` | quaere (sine discrimine magnitudinis) |
| `n` | proximum inventum |
| `N` | prius inventum |
| `v` | lancia editorem (`$EDITOR`, alioqui `vi`) ad lineam currentem |
| `q` | exi |

## Lineamenta

- Alveus alter (scrollback non polluitur)
- Redimensio fenestrae per SIGWINCH
- Investigatio cum luce flava in locis inventis
- Circumvolutio quaesiti (a fine ad initium et vice versa)
- Expansio tabularum, characteres moderantes ut `^X`
- Lectio ex tubo per `/dev/tty`
- Barra status cum nomine, positione, centesima

## Plicae

```
plus.c          — fons unicus
Faceplica       — aedificatio
```

## Dependentiae

Nullae. C et POSIX solum.
