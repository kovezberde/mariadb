/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Tell me the diff between two brt files. */

#include "includes.h"
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>

static void
format_time(const uint64_t time_int, char *buf) {
    time_t timer = (time_t) time_int;
    ctime_r(&timer, buf);
    assert(buf[24] == '\n');
    buf[24] = 0;
}

static int dump_data = 1;

static CACHETABLE ct;

static void
print_item (bytevec val, ITEMLEN len) {
    printf("\"");
    ITEMLEN i;
    for (i=0; i<len; i++) {
	unsigned char ch = ((unsigned char*)val)[i];
	if (isprint(ch) && ch!='\\' && ch!='"') {
	    printf("%c", ch);
	} else {
	    printf("\\%03o", ch);
	}
    }
    printf("\"");
}

static void
simple_hex_dump(unsigned char *vp, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        unsigned char c = vp[i];
        printf("%2.2X", c);
    }
}

static void
hex_dump(unsigned char *vp, uint64_t offset, uint64_t size) {
    uint64_t n = size / 32;
    for (uint64_t i = 0; i < n; i++) {
	printf("%" PRIu64 ": ", offset);
	for (uint64_t j = 0; j < 32; j++) {
	    unsigned char c = vp[j];
	    printf("%2.2X", c);
	    if (((j+1) % 4) == 0)
		printf(" ");
	}
	for (uint64_t j = 0; j < 32; j++) {
	    unsigned char c = vp[j];
	    printf("%c", isprint(c) ? c : ' ');
	}
	printf("\n");
	vp += 32;
	offset += 32;
    }
    size = size % 32;
    for (uint64_t i=0; i<size; i++) {
	if ((i % 32) == 0)
	    printf("%" PRIu64 ": ", offset+i);
	printf("%2.2X", vp[i]);
	if (((i+1) % 4) == 0)
	    printf(" ");
	if (((i+1) % 32) == 0)
	    printf("\n");
    }
    printf("\n");
}

static void
dump_descriptor(DESCRIPTOR d) {
    printf(" descriptor size %u ", d->dbt.size);
    simple_hex_dump((unsigned char*) d->dbt.data, d->dbt.size);
    printf("\n");
}

static void
dump_header (int f, FT *header, CACHEFILE cf) {
    FT ft = NULL;
    int r;
    char timestr[26];
    r = toku_deserialize_ft_from (f, MAX_LSN, &ft);
    assert(r==0);
    ft->cf = cf;
    printf("ft:\n");
    printf(" layout_version=%d\n", ft->h->layout_version);
    printf(" layout_version_original=%d\n", ft->h->layout_version_original);
    printf(" layout_version_read_from_disk=%d\n", ft->layout_version_read_from_disk);
    printf(" build_id=%d\n", ft->h->build_id);
    printf(" build_id_original=%d\n", ft->h->build_id_original);
    format_time(ft->h->time_of_creation, timestr);
    printf(" time_of_creation=         %" PRIu64 "    %s\n", ft->h->time_of_creation, timestr);
    format_time(ft->h->time_of_last_modification, timestr);
    printf(" time_of_last_modification=%" PRIu64 "    %s\n", ft->h->time_of_last_modification, timestr);
    printf(" dirty=%d\n", ft->h->dirty);
    printf(" checkpoint_count=%" PRId64 "\n", ft->h->checkpoint_count);
    printf(" checkpoint_lsn=%" PRId64 "\n", ft->h->checkpoint_lsn.lsn);
    printf(" nodesize=%u\n", ft->h->nodesize);
    printf(" basementnodesize=%u\n", ft->h->basementnodesize);
    printf(" compression_method=%u\n", (unsigned) ft->h->compression_method);
    printf(" unnamed_root=%" PRId64 "\n", ft->h->root_blocknum.b);
    printf(" flags=%u\n", ft->h->flags);
    dump_descriptor(&ft->descriptor);
    printf(" estimated numrows=%" PRId64 "\n", ft->in_memory_stats.numrows);
    printf(" estimated numbytes=%" PRId64 "\n", ft->in_memory_stats.numbytes);
    *header = ft;
}

static int
print_le (OMTVALUE lev, uint32_t UU(idx), void *UU(v)) {
    LEAFENTRY CAST_FROM_VOIDP(le, lev);
    print_leafentry(stdout, le);
    printf("\n");
    return 0;
}

static void
dump_node (int f, BLOCKNUM blocknum, FT h) {
    FTNODE n;
    struct ftnode_fetch_extra bfe;
    FTNODE_DISK_DATA ndd = NULL;
    fill_bfe_for_full_read(&bfe, h);
    int r = toku_deserialize_ftnode_from (f, blocknum, 0 /*pass zero for hash, it doesn't matter*/, &n, &ndd, &bfe);
    assert(r==0);
    assert(n!=0);
    printf("ftnode\n");
    DISKOFF disksize, diskoffset;
    toku_translate_blocknum_to_offset_size(h->blocktable, blocknum, &diskoffset, &disksize);
    printf(" diskoffset  =%" PRId64 "\n", diskoffset);
    printf(" disksize    =%" PRId64 "\n", disksize);
    printf(" serialize_size =%u\n", toku_serialize_ftnode_size(n));
    printf(" flags       =%u\n", n->flags);
    printf(" thisnodename=%" PRId64 "\n", n->thisnodename.b);
    //printf(" log_lsn     =%lld\n", n->log_lsn.lsn); // The log_lsn is a memory-only value.
    printf(" height      =%d\n",   n->height);
    printf(" layout_version=%d\n", n->layout_version);
    printf(" layout_version_original=%d\n", n->layout_version_original);
    printf(" layout_version_read_from_disk=%d\n", n->layout_version_read_from_disk);
    printf(" build_id=%d\n", n->build_id);
    printf(" max_msn_applied_to_node_on_disk=%" PRId64 " (0x%" PRIx64 ")\n", n->max_msn_applied_to_node_on_disk.msn, n->max_msn_applied_to_node_on_disk.msn);

    printf(" n_children=%d\n", n->n_children);
    printf(" total_childkeylens=%u\n", n->totalchildkeylens);

    printf(" pivots:\n");
    for (int i=0; i<n->n_children-1; i++) {
        const DBT *piv = &n->childkeys[i];
        printf("  pivot %2d:", i);
        assert(n->flags == 0);
        print_item(piv->data, piv->size);
        printf("\n");
    }
    printf(" children:\n");
    for (int i=0; i<n->n_children; i++) {
        if (n->height > 0) {
            printf("   child %d: %" PRId64 "\n", i, BP_BLOCKNUM(n, i).b);
            NONLEAF_CHILDINFO bnc = BNC(n, i);
            unsigned int n_bytes = toku_bnc_nbytesinbuf(bnc); 
            int n_entries = toku_bnc_n_entries(bnc);
            if (n_bytes > 0 || n_entries > 0) {
                printf("   buffer contains %u bytes (%d items)\n", n_bytes, n_entries);
            }
            if (dump_data) {
                FIFO_ITERATE(bnc->buffer, key, keylen, data, datalen, typ, msn, xids, UU(is_fresh),
                             {
                                 printf("    msn=%" PRIu64 " (0x%" PRIx64 ") ", msn.msn, msn.msn);
                                 printf("    TYPE=");
                                 switch ((enum ft_msg_type)typ) {
                                 case FT_NONE: printf("NONE"); goto ok;
                                 case FT_INSERT: printf("INSERT"); goto ok;
                                 case FT_INSERT_NO_OVERWRITE: printf("INSERT_NO_OVERWRITE"); goto ok;
                                 case FT_DELETE_ANY: printf("DELETE_ANY"); goto ok;
                                 case FT_ABORT_ANY: printf("ABORT_ANY"); goto ok;
                                 case FT_COMMIT_ANY: printf("COMMIT_ANY"); goto ok;
                                 case FT_COMMIT_BROADCAST_ALL: printf("COMMIT_BROADCAST_ALL"); goto ok;
                                 case FT_COMMIT_BROADCAST_TXN: printf("COMMIT_BROADCAST_TXN"); goto ok;
                                 case FT_ABORT_BROADCAST_TXN: printf("ABORT_BROADCAST_TXN"); goto ok;
                                 case FT_OPTIMIZE: printf("OPTIMIZE"); goto ok;
                                 case FT_OPTIMIZE_FOR_UPGRADE: printf("OPTIMIZE_FOR_UPGRADE"); goto ok;
                                 case FT_UPDATE:   printf("UPDATE"); goto ok;
                                 case FT_UPDATE_BROADCAST_ALL: printf("UPDATE_BROADCAST_ALL"); goto ok;
                                 }
                                 printf("HUH?");
                             ok:
                                 printf(" xid=");
                                 xids_fprintf(stdout, xids);
                                 printf(" ");
				 print_item(key, keylen);
				 if (datalen>0) {
				     printf(" ");
				     print_item(data, datalen);
				 }
				 printf("\n");
			     }
			     );
	    }
	} else {
	    printf(" n_bytes_in_buffer=%u", BLB_NBYTESINBUF(n, i));
	    printf(" items_in_buffer=%u\n", toku_omt_size(BLB_BUFFER(n, i)));
	    if (dump_data) toku_omt_iterate(BLB_BUFFER(n, i), print_le, 0);
	}
    }
    toku_ftnode_free(&n);
    toku_free(ndd);
}

static void 
dump_block_translation(FT h, uint64_t offset) {
    toku_blocknum_dump_translation(h->blocktable, make_blocknum(offset));
}

typedef struct {
    int f;
    FT h;
    uint64_t blocksizes;
    uint64_t leafsizes;
    uint64_t leafblocks;
} frag_help_extra;

static int
fragmentation_helper(BLOCKNUM b, int64_t size, int64_t UU(address), void *extra) {
    frag_help_extra *CAST_FROM_VOIDP(info, extra);
    FTNODE n;
    FTNODE_DISK_DATA ndd = NULL;
    struct ftnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, info->h);
    int r = toku_deserialize_ftnode_from(info->f, b, 0 /*pass zero for hash, it doesn't matter*/, &n, &ndd, &bfe);
    if (r==0) {
        info->blocksizes += size;
        if (n->height == 0) {
            info->leafsizes += size;
            info->leafblocks++;
        }
	toku_ftnode_free(&n);
        toku_free(ndd);
    }
    return 0;
}

static void 
dump_fragmentation(int f, FT h) {
    frag_help_extra info;
    memset(&info, 0, sizeof(info));
    info.f = f;
    info.h = h;
    toku_blocktable_iterate(h->blocktable, TRANSLATION_CHECKPOINTED,
                            fragmentation_helper, &info, true, true);
    int64_t used_space;
    int64_t total_space;
    toku_blocktable_internal_fragmentation(h->blocktable, &total_space, &used_space);
    int64_t fragsizes = total_space - used_space;

    printf("leafblocks: %" PRIu64 "\n", info.leafblocks);
    printf("blocksizes: %" PRIu64 "\n", info.blocksizes);
    printf("used size: %" PRId64 "\n",  used_space);
    printf("total size: %" PRId64 "\n", total_space);
    printf("leafsizes: %" PRIu64 "\n", info.leafsizes);
    printf("fragsizes: %" PRId64 "\n", fragsizes);
    printf("fragmentation: %.1f%%\n", 100. * ((double)fragsizes / (double)(total_space)));
}

typedef struct {
    int f;
    FT h;
    size_t total_space;
    size_t used_space;
} garbage_help_extra;

static int
garbage_leafentry_helper(OMTVALUE v, uint32_t UU(idx), void *extra) {
    garbage_help_extra *CAST_FROM_VOIDP(info, extra);
    LEAFENTRY CAST_FROM_VOIDP(le, v);
    info->total_space += leafentry_disksize(le);
    info->used_space += LE_CLEAN_MEMSIZE(le_latest_keylen(le), le_latest_vallen(le));
    return 0;
}

static int
garbage_helper(BLOCKNUM b, int64_t UU(size), int64_t UU(address), void *extra) {
    garbage_help_extra *CAST_FROM_VOIDP(info, extra);
    FTNODE n;
    FTNODE_DISK_DATA ndd = NULL;
    struct ftnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, info->h);
    int r = toku_deserialize_ftnode_from(info->f, b, 0, &n, &ndd, &bfe);
    if (r != 0) {
        goto no_node;
    }
    if (n->height > 0) {
        goto exit;
    }
    for (int i = 0; i < n->n_children; ++i) {
        BASEMENTNODE bn = BLB(n, i);
        r = toku_omt_iterate(bn->buffer, garbage_leafentry_helper, info);
        if (r != 0) {
            goto exit;
        }
    }
exit:
    toku_ftnode_free(&n);
    toku_free(ndd);
no_node:
    return r;
}

static void
dump_garbage_stats(int f, FT h) {
    garbage_help_extra info;
    memset(&info, 0, sizeof info);
    info.f = f;
    info.h = h;
    toku_blocktable_iterate(h->blocktable, TRANSLATION_CHECKPOINTED,
                            garbage_helper, &info, true, true);

    printf("total_size: %zu\n", info.total_space);
    printf("used_size:  %zu\n", info.used_space);
}

static uint32_t 
get_unaligned_uint32(unsigned char *p) {
    return *(uint32_t *)p;
}

struct dump_sub_block {
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint32_t xsum;
};

static void
sub_block_deserialize(struct dump_sub_block *sb, unsigned char *sub_block_header) {
    sb->compressed_size = toku_dtoh32(get_unaligned_uint32(sub_block_header+0));
    sb->uncompressed_size = toku_dtoh32(get_unaligned_uint32(sub_block_header+4));
    sb->xsum = toku_dtoh32(get_unaligned_uint32(sub_block_header+8));
}

static void
verify_block(unsigned char *cp, uint64_t file_offset, uint64_t size) {
    // verify the header checksum
    const size_t node_header = 8 + sizeof (uint32_t) + sizeof (uint32_t) + sizeof (uint32_t);
    
    printf("%.8s layout_version=%u %u build=%d\n", cp, get_unaligned_uint32(cp+8), get_unaligned_uint32(cp+12), get_unaligned_uint32(cp+16));

    unsigned char *sub_block_header = &cp[node_header];
    uint32_t n_sub_blocks = toku_dtoh32(get_unaligned_uint32(&sub_block_header[0]));
    uint32_t header_length = node_header + n_sub_blocks * sizeof (struct dump_sub_block);
    header_length += sizeof (uint32_t); // CRC
    if (header_length > size) {
        printf("header length too big: %u\n", header_length);
        return;
    }
    uint32_t header_xsum = x1764_memory(cp, header_length);
    uint32_t expected_xsum = toku_dtoh32(get_unaligned_uint32(&cp[header_length]));
    if (header_xsum != expected_xsum) {
        printf("header checksum failed: %u %u\n", header_xsum, expected_xsum);
        return;
    }

    // deserialize the sub block header
    struct dump_sub_block sub_block[n_sub_blocks];
    sub_block_header += sizeof (uint32_t);
    for (uint32_t i = 0 ; i < n_sub_blocks; i++) {
        sub_block_deserialize(&sub_block[i], sub_block_header);
        sub_block_header += sizeof (struct dump_sub_block);
    }

    // verify the sub block header
    uint32_t offset = header_length + 4;
    for (uint32_t i = 0 ; i < n_sub_blocks; i++) {
        uint32_t xsum = x1764_memory(cp + offset, sub_block[i].compressed_size);
        printf("%u: %u %u %u", i, sub_block[i].compressed_size, sub_block[i].uncompressed_size, sub_block[i].xsum);
        if (xsum != sub_block[i].xsum)
            printf(" fail %u offset %" PRIu64, xsum, file_offset + offset);
        printf("\n");
        offset += sub_block[i].compressed_size;
    }
    if (offset != size)
        printf("offset %u expected %" PRIu64 "\n", offset, size);
}

static void
dump_block(int f, BLOCKNUM blocknum, FT h) {
    DISKOFF offset, size;
    toku_translate_blocknum_to_offset_size(h->blocktable, blocknum, &offset, &size);
    printf("%" PRId64 " at %" PRId64 " size %" PRId64 "\n", blocknum.b, offset, size);

    unsigned char *CAST_FROM_VOIDP(vp, toku_malloc(size));
    uint64_t r = pread(f, vp, size, offset);
    if (r == (uint64_t)size) {
        verify_block(vp, offset, size);
    }
    toku_free(vp);
}

static void
dump_file(int f, uint64_t offset, uint64_t size, FILE *outfp) {
    unsigned char *XMALLOC_N(size, vp);
    uint64_t r = pread(f, vp, size, offset);
    if (r == size) {
        if (outfp == stdout)
            hex_dump(vp, offset, size);
        else
            fwrite(vp, size, 1, outfp);
    }
    toku_free(vp);
}

static void
set_file(int f, uint64_t offset, unsigned char newc) {
    toku_os_pwrite(f, &newc, sizeof newc, offset);
}

static int
readline (char *line, int maxline) {
    int i = 0;
    int c;
    while ((c = getchar()) != EOF && c != '\n' && i < maxline) {
        line[i++] = (char)c;
    }
    line[i++] = 0;
    return c == EOF ? EOF : i;
}

static int
split_fields (char *line, char *fields[], int maxfields) {
    int i;
    for (i=0; i<maxfields; i++, line=NULL) {
        fields[i] = strtok(line, " ");
        if (fields[i] == NULL) {
            for (; i < maxfields; ++i) {
                fields[i] = NULL;
            }
            break;
        }
    }
    return i;
}

static int
usage(const char *arg0) {
    printf("Usage: %s [--nodata] [--interactive] [--rootnode] ftfilename\n", arg0);
    return 1;
}

typedef struct __dump_node_extra {
    int f;
    FT h;
} dump_node_extra;

static int
dump_node_wrapper(BLOCKNUM b, int64_t UU(size), int64_t UU(address), void *extra) {
    dump_node_extra *CAST_FROM_VOIDP(info, extra);
    dump_node(info->f, b, info->h);
    return 0;
}

static void 
interactive_help(void) {
    fprintf(stderr, "help\n");
    fprintf(stderr, "header\n");
    fprintf(stderr, "node NUMBER\n");
    fprintf(stderr, "bx OFFSET | block_translation OFFSET\n");
    fprintf(stderr, "dumpdata 0|1\n");
    fprintf(stderr, "fragmentation\n");
    fprintf(stderr, "garbage\n");
    fprintf(stderr, "file OFFSET SIZE [outfilename]\n");
    fprintf(stderr, "quit\n");
}

static uint64_t
getuint64(const char *f) {
    if (strncmp(f, "0x", 2) == 0 || strncmp(f, "0X", 2) == 0)
        return strtoull(f, 0, 16);
    else if (strncmp(f, "0", 1) == 0)
        return strtoull(f, 0, 8);
    else
        return strtoull(f, 0, 10);
}

int 
main (int argc, const char *const argv[]) {
    int interactive = 0;
    int rootnode = 0;

    const char *arg0 = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "--nodata") == 0) {
	    dump_data = 0;
	} else if (strcmp(argv[0], "--interactive") == 0 || strcmp(argv[0], "--i") == 0) {
	    interactive = 1;
	} else if (strcmp(argv[0], "--rootnode") == 0) {
	    rootnode = 1;
	} else if (strcmp(argv[0], "--help") == 0) {
	    return usage(arg0);
	} else 
	    break;
	argc--; argv++;
    }
    if (argc != 1) return usage(arg0);

    int r = toku_ft_layer_init();
    invariant_zero(r);

    const char *n = argv[0];
    int f = open(n, O_RDWR + O_BINARY);  assert(f>=0);
    FT ft;
    // create a cachefile for the header
    toku_cachetable_create(&ct, 1<<25, (LSN){0}, 0);
    CACHEFILE cf = NULL;
    r = toku_cachetable_openfd (&cf, ct, f, n);
    assert(r==0);
    dump_header(f, &ft, cf);
    if (interactive) {
        while (1) {
            printf("ftdump>"); fflush(stdout);
	    enum { maxline = 64};
	    char line[maxline+1];
	    r = readline(line, maxline);
	    if (r == EOF)
		break;
	    const int maxfields = 4;
	    char *fields[maxfields];
	    int nfields = split_fields(line, fields, maxfields);
	    if (nfields == 0) 
		continue;
	    if (strcmp(fields[0], "help") == 0) {
		interactive_help();
	    } else if (strcmp(fields[0], "header") == 0) {
		toku_ft_free(ft);
		dump_header(f, &ft, cf);
	    } else if (strcmp(fields[0], "block") == 0 && nfields == 2) {
		BLOCKNUM blocknum = make_blocknum(getuint64(fields[1]));
		dump_block(f, blocknum, ft);
	    } else if (strcmp(fields[0], "node") == 0 && nfields == 2) {
		BLOCKNUM off = make_blocknum(getuint64(fields[1]));
		dump_node(f, off, ft);
	    } else if (strcmp(fields[0], "dumpdata") == 0 && nfields == 2) {
		dump_data = strtol(fields[1], NULL, 10);
	    } else if (strcmp(fields[0], "block_translation") == 0 || strcmp(fields[0], "bx") == 0) {
		uint64_t offset = 0;
		if (nfields == 2)
		    offset = getuint64(fields[1]);
		dump_block_translation(ft, offset);
	    } else if (strcmp(fields[0], "fragmentation") == 0) {
		dump_fragmentation(f, ft);
            } else if (strcmp(fields[0], "garbage") == 0) {
                dump_garbage_stats(f, ft);
	    } else if (strcmp(fields[0], "file") == 0 && nfields >= 3) {
		uint64_t offset = getuint64(fields[1]);
		uint64_t size = getuint64(fields[2]);
		FILE *outfp = stdout;
		if (nfields >= 4)
		    outfp = fopen(fields[3], "w");
		dump_file(f, offset, size, outfp);
	    } else if (strcmp(fields[0], "setfile") == 0 && nfields == 3) {
		uint64_t offset = getuint64(fields[1]);
		unsigned char newc = getuint64(fields[2]);
		set_file(f, offset, newc);
	    } else if (strcmp(fields[0], "quit") == 0 || strcmp(fields[0], "q") == 0) {
		break;
	    }
	}
    } else if (rootnode) {
	dump_node(f, ft->h->root_blocknum, ft);
    } else {
	printf("Block translation:");

	toku_dump_translation_table(stdout, ft->blocktable);

	struct __dump_node_extra info;
	info.f = f;
	info.h = ft;
	toku_blocktable_iterate(ft->blocktable, TRANSLATION_CHECKPOINTED,
				dump_node_wrapper, &info, true, true);
    }
    toku_ft_free(ft);
    toku_ft_layer_destroy();
    return 0;
}
