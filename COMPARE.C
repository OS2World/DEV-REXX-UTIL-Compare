/*
                               COMPARE.C
             Copyright (C) Seven Valleys Software 1985,1988
         Written for Seven Valleys Software by Cheyenne Wills &
                    Released For Public Distribution
                          All Rights Reserved

  Permission is granted to freely distribute this code, but not for
profit, provided that this notice and the following disclaimer are
included in their entirety and without modifications of any sort.  This
work may not be sold, or modified and sold, or included in any other
work to be sold, (except for a nominal media charge), without the
written permission of the author.

  Permission is granted to modify the source code and distribute it in
modified form PROVIDED that the authors of any modifications identify
themselves with name and address following this header and that all such
modifications are clearly indicated as to location and purpose, with
descriptive comments that clearly indicate modified lines.

  The author would appreciate hearing of any modifications that may be
made, but makes no guarantees that such modifications will be
distributed with future releases of this program.

Author's address:

        Cheyenne C. Wills
        12 West Locust St.
        Mechanicsburg, Pa. 17055
        (717) 697-5198

        Written for
        Seven Valley's Software
        P.O. Box 99
        Glen Rock, Pa 17327

Disclaimer:

  No guarantees or warranties of any kind: This code is distributed
"AS IS" without any warranty of any kind, either expressed or implied,
including, but not limited to the implied warranties of merchantability
and fitness for a particular purpose.  You are soley responsible for the
selection of the program to achieve your intended results and for the
results actually obtained.  Should the program prove defective, you (and
not the author) assume the entire cost of all necessary servicing,
repair, or correction.

  Neither the author nor anyone else who has been involved in the
creation, production or delivery of this program shall be liable for any
direct, indirect, consequential or incidental damages arising out of the
use or inability to use this program.

                               History:
Version    Date    Author                 Reason
-------  --------  ------  ---------------------------------------------
 2.0     08/01/84   CCW    Written for Seven Valleys Software
 2.1     08/15/85   CCW    Added -i option and online doc
 2.2     02/05/86   CCW    Added -t and -w options
 2.3     01/21/88   CCW    OS/2 Support
 2.4     11/21/88   CCW    Make more portable (ANSI)
 2.5     11/19/91   JPW    cleaned up indents, added descriptive report
 2.6     01/14/92   JPW    fixed an eof bug, misc more cleanup, drop 1st ff
*/

char *VERSION = "COMPARE V2.6\n";
char *COPYR   = "Copyright (C) Seven Valleys Software 1985,1988\n";

/*

COMPARE <file1> <file2> [options]

Where:

<file1> and <file2> are the two files to be compared

[options] are:

-l [<listing>]  Generate output to file
                <listing> defaults to COMPARE.LST
-w              Ignore white space between 'words'
-t              Ignore case
-p              Generate output to the printer
-mn             Resync on n lines (default is 4)
-cx,y           Only compare cols x thru y

------------ following for update deck generation only ------------
-u [<update deck>] Generate update deck to make file1 into file2
                <update deck> defaults to the <name of file1>."UPT"
-sx,y           Generate internal sequence numbers starting with x
                and increment by y. Default if -s is omitted is 1,1
                if -s is specified with out x and y then x=10000
                y defaults to x
-x              File1 already contains sequence numbers
------------ following for aide with editor shells only -----------
-i              Use a sequence file (generated from UPDATE command)
                name is <name of file1>."SEQ"
                will be deleted
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#define MAXLEN 256

int MINMATCH = 4;

struct line
{
    struct line *next;  /* Pointer to the next line */
    long seqnum;    /* Current sequence number */
    long len;       /* Length of the current line */
    char *text;     /* Pointer to the current line */
};

struct fanchor
{
    struct line *curr;  /* Pointer to the current line buffer */
    struct line *head;  /* Pointer to the head of the list */
    struct line *tail;  /* Pointer to the last line in storage */
    long cline, hline, tline; /* Line numbers of Current, Head, & Tail */
    char eof;           /* If EOF on this file then TRUE */
};

struct
{        /* Temp */
    long len;
    char text[512];
} templine;

struct fanchor a,b;

FILE *outlist, *in0, *in1, *seqf;
FILE *console;
#define IOBUFSZ 4096
char iobuf0[IOBUFSZ];
char iobuf1[IOBUFSZ];

char updtname[64];              /* Update deck name */
char seqname[64];               /* Name of sequence file */
char *files[3];                 /* Pointers to the names of the files */

char Match, Eof, Different, Edit, Print, ToAFile, XSeqf, Iwhite, Icase;

long seqstrt = 1;       /* Sequencing start */
long seqincr = 1;       /* Sequencing increment */
long seqcur  = 0;       /* Current sequence number */
long lastmseq = 0;      /* Last matched sequence number */

int colstart = 1;       /* Column starting position for compares */
int colend = MAXLEN+1;  /* Column ending position for compares */

char Seqtype = 0;       /* Type of sequencing */
#define GenSeq  0       /* Generate the sequence numbers (use 10000 10000) */
#define FileSeq 1       /* Seq numbers are already in the file (first 9 pos) */
#define UserSeq 2       /* Use sequence start and increment specified by user */
#define SeqFile 3

#define LINESPERPAGE 58
int curline = 52;   /* LINESPERPAGE+1; ---removed to prevent initial ff */
                    /* this should give a few lines in addition to the  */
                    /* tlib header before getting to the 'meat'         */

/*----------------------------------------------------------------------------*/
/* {main: Main compare routine                                                */
/*                                                                            */
/*   Original Author: CCW                                                     */
/*----------------------------------------------------------------------------*/
main(argc,argv)
int argc;
char *argv[];
{
    char i, *cp;
    int fi;

    console=stdout;

    /* Handle the quiet option */
    if(strcmp(argv[argc-1],"-q")==0)
    {
        console=fopen("NUL","w");
        argc--;
    }
    else
    {
        fputs(VERSION,stderr);   /* Put out our header for version */
        fputs(COPYR,stderr);     /* Put out our header for version */
        fflush(stderr);
    }

    /* We need at least two files */
    if(argc<3)
    {   /* Tell about the error */
        fputs("\nMissing filename(s).\n",stderr);
        cmndformat();
        exit(12);
    }


    fi=0;
    if(*argv[1]!='-')  files[fi++]=argv[1];
    if(*argv[2]!='-')  files[fi++]=argv[2];

    for(i=3;i<argc;i++)
    {   /* process the command line options */
        cp=argv[i];
        if(*cp != '-')
        {
            fprintf(stderr,"\nInvalid argument %s",cp);
            cmndformat();
            exit(12);
        }
        cp++;
        switch(tolower(*cp))
        {
            case 't':
            Icase++;
            break;
            case 'w':
            Iwhite++;
            break;
            case 'i':
            Seqtype=SeqFile;
            XSeqf++;
            break;
            case 'p':       /* handle going to the printer */
            if(fi<3)
                files[fi++]="PRN:";
            Print++;
            break;
            case 'l':       /* use an output file */
            cp=argv[i+1];
            if(i+1<argc && *cp !='-'&& fi<3)
            {
                files[fi++]=cp;
                i++;
            }
            else
                if(i+1==argc || *cp=='-'&& fi<3)
                    files[fi++]="COMPARE.LST";
            break;
        case 'u':
            cp=argv[i+1];
            if(i+1<argc && *cp != '-'&& fi<3)
            {
                files[fi++]=cp;
                i++;
            }
            Edit++;
            break;
        case 's':
            cp++;
            Seqtype=UserSeq;
            if(isdigit(*cp))
            {  /* Handle a number */
                seqstrt = atol(cp);
                cp += strspn(cp,"0123456789");
                if(*cp==',')
                {
                    cp++;
                    seqincr = atol(cp);
                }
                    else seqincr=seqstrt;
            }
            else
                seqstrt=seqincr=10000;
            seqcur=seqstrt-seqincr;
            break;
        case 'x':
            cp++;
            Seqtype=FileSeq;
            seqincr=10000;
            break;
        case 'c':
            cp++;
            if( isdigit(*cp) )
            {  /* Handle a number */
                colstart = atoi(cp);
                cp += strspn(cp, "0123456789");
                if(*cp==',')
                {
                    cp++;
                    colend = atoi(cp);
                }
            }
            else
            {
                fputs("\nMissing starting and ending column positions.\n",stderr);
                cmndformat();
                exit(12);
            }
            colend=colend-colstart+1;
            break;
        case 'm':
            cp++;
            if( isdigit(*cp) )
            {  /* Handle a number */
                MINMATCH = atoi(cp);
                cp += strspn(cp, "0123456789");
            }
            else
            {
                fputs("\nInvalid Match value.\n",stderr);
                cmndformat();
                exit(12);
            }
            break;
            default:
            break;
        }    /* switch */
    }    /* for loop process args */

    i=fi;
    /* We need at least two files */
    if(i<2)
    {   /* Tell about the error */
        fputs("\nMissing filename(s).\n",stderr);
        cmndformat();
        exit(12);
    }
    /* Now check the options specified */
    if( Print && Edit )
    {   /* Too many options specified */
        fputs("\nConflicting options -u and -p\n",stderr);
        cmndformat();
        exit(12);
    }
    if( Print && i>3 )
    {   /* Print and out file are conflicting */
        fputs("\nConflicting options -p and output file\n",stderr);
        cmndformat();
        exit(12);
    }

    if( (in0=fopen(files[0],"r"))==NULL)
    {   /* Error getting to file1 */
        fprintf(stderr,"\nUnable to open file1 [%s]\n",files[0]);
        exit(12);
    }
    if( (in1=fopen(files[1],"r"))==NULL)
    {   /* Error getting to file2 */
        fprintf(stderr,"\nUnable to open file2 [%s]\n",files[1]);
        exit(12);
    }

    newext(updtname,files[0],"upt"); /* Generate the alt name now */
    newext(seqname,files[0],"seq");  /* Generate the seq name now */

    if(i==2)
    {
        files[2]="CON";
        ToAFile=1;   /* added to print header to con also */
    }


    if(i==3)
        ToAFile=1;

    if(Edit&&i==2)
    {
        files[2]=updtname;
        fprintf(console,"\nCreating update file [%s]\n",updtname);
    }
    if(Print)
        files[2]="PRN";

    if( (outlist=fopen(files[2],"w"))==NULL)
    {   /* Error opening file */
        fprintf(stderr,"\nUnable to create output file [%s]\n",files[2]);
        exit(12);
    }

    if(XSeqf && (seqf=fopen(seqname,"rb"))==NULL)
    {
       /* Error opening file */
       fprintf(stderr,"\nUnable to read sequence file [%s]\n",seqname);
       exit(12);
    }

    /* Beef up performance some */
    setvbuf(in0,iobuf0,_IOFBF,IOBUFSZ);
    setvbuf(in1,iobuf1,_IOFBF,IOBUFSZ);

    initialize();

    if(a.eof)
        fprintf(stderr,"File [%s] is empty.\n",argv[1]);

    if(b.eof)
        fprintf(stderr,"File [%s] is empty.\n",argv[2]);

    if(!Eof)
    {
        Different=0;
        compf();
        if(!Different) fputs("The two files are identical.\n",console);
        }
        fclose(in0);
        fclose(in1);
        if(XSeqf)
        {
            fclose(seqf);   /* Close the sequence file */
            unlink(seqname); /* and get rid of it */
        }
        fflush(outlist);
        fclose(outlist);

        if(Different)
            exit(4);  /* Indicate that things are different */

        exit(0);
    }

compf()
{
    Match=1;    /* I.E. Beginnings of files match */
    for(;;)
    {
    if(Match)
        fnoteq();
    else
    {
            Different=1;
            findeq();
        }
    if(Eof&&Match)
        break;
    }
}

endfanchor(x)
struct fanchor *x;
{
    return( (x->curr==NULL)&&x->eof );
}

mark(x) /* Causes beginning of fanchor to be positioned before current */
        /* fanchor curr.  buffers get reclaimed, line counters reset */
        /* etc. */
struct fanchor *x;
{
    struct line *p;

    if(x->head!=NULL)
    {
        while(x->head!=x->curr)
        {   /* Reclaim buffers */
            p=x->head->next;
            free(x->head->text);
            free(x->head);
            x->head=p;
        }
        x->hline=x->cline;
        if(x->curr==NULL)
        {
            x->tail=NULL;
            x->tline=x->cline;
        }
    }
}

movecurr(x,filex) /* Filex is the input file associated with fanchor x */
struct fanchor *x;   /* The curr for x is moved forward one line, reading */
FILE *filex;        /* X if necessary, and incrementing the line count. */
{                   /* eof is set of eof is encountered on either fanchor */
    if(x->curr)
    {
        if(x->curr==x->tail)
            readline(x,filex);
        x->curr=x->curr->next;
        if(x->curr==NULL)
            Eof=1;
        x->cline++;
    }
    else
        if(!x->eof)
        {
            readline(x,filex);
            x->curr=x->head;
            x->cline=x->hline;
        }
        else
            Eof=1;
}

readline(x,filex)
struct fanchor *x;
FILE *filex;
{
    struct line *newline;
    char *c;
    long int l;
    int i;
    if(!x->eof)
    {
        if( fgets(templine.text,MAXLEN,filex) )
        {
            i = strlen(templine.text);      /* Ensure that there is */
            if(templine.text[i] != '\n')
            {  /* a newline at the end */
                templine.text[i+1] = '\n';
                templine.text[i+2] = '\0';
            }
            c=templine.text;    /* Point to the start of the text */
            if(Edit && filex==in0)
            {
                switch(Seqtype)
                {   /* Generate the sequence number */
                case GenSeq:    /* We are to generate the sequence */

                case UserSeq:
                    seqcur+=seqincr;   /* Increment */
                    break;

                case FileSeq:   /* The first item in the file is a number */
                    l=seqcur;
                    seqcur = atol(templine.text);
                    c += strspn(templine.text,"0123456789");
                    if(seqcur!=0)
                        seqincr=seqcur-l;
                    if(*c!='\n')
                        ++c; /* Handle a null line */
                    break;

                case SeqFile:
                    l=seqcur;
                    fread(&seqcur,sizeof(seqcur),1,seqf);
                    if( feof(seqf) || ferror(seqf) )
                    {
                        fprintf(stderr,"\nPremature EOF on sequence file [%s]\n",seqname);
                        exit(12);
                    }
                    if(seqcur!=0) seqincr=seqcur-l;

                default:
                    break;
                }
            }
            templine.len=strlen(c)+1; /* Get the len of the line */

            newline=(struct line *)malloc(sizeof(struct line));
            if(newline==NULL)
            { /* Out of memory */
                fputs("\nInsuffecient Memory\n",stderr);
                exit(12);
            }

            newline->text=malloc((short)templine.len);

            if(newline->text==NULL)
            { /* Out of memory */
                fputs("\nInsuffecient Memory\n",stderr);
                exit(12);
            }
            strcpy(newline->text,c);
            newline->seqnum=seqcur;    /* Get the current sequence number */
            newline->len=templine.len;
            newline->next=NULL;
            if(x->tail==NULL)
            {
                x->head=newline;
                x->tline=1;
                x->hline=1;
            }
            else
            {
                x->tail->next=newline;
                x->tline++;
            }
            x->tail=newline;
        }
        x->eof = feof(filex);
    }
}

backtrack(x, xlines)/* Causes the current position of fanchor X to become */
struct fanchor *x;  /* that of the last mark operation.  I.E. the current */
int *xlines;        /* line when the fanchor was marked last becomes the */
{                   /* the new curr. XLINES is set to the number of lines */
                    /* from the new curr to the old curr inclusive */
    *xlines=x->cline+1-x->hline;
    x->curr=x->head;
    x->cline=x->hline;
    Eof=endfanchor(&a) || endfanchor(&b);
}
compl() /* Compare the current lines of fanchors a and b */
        /* returning match to signal their (non-) equivalence */
        /* EOF on both files is considered a match, but eof on */
        /* only one is a mismatch */
{
    int cea,ceb;    /* Ending columns for file a and file b */
    int i;
    char *ca,*cb;

    if(a.curr==NULL || b.curr==NULL)
        Match=(endfanchor(&a) && endfanchor(&b));
    else
    {
        if(Iwhite)
        {
            Match = compx(a.curr,b.curr);
            return 0;    /* added 0 to prevent warning */
        }
        cea = min(a.curr->len,colend);
        ceb = min(b.curr->len,colend);
        Match=(cea==ceb);   /* Equal lens */
        if(Match)
        { /* The lengths match.. so lets see if we can compare them */
            if(cea<=colstart)
               Match=0;
            else
            {
                ca=a.curr->text;
                cb=b.curr->text;
                ca+=colstart-1;
                cb+=colstart-1;
                for(i=colstart;i<cea;i++)
                {
                    if(Icase)
                    {
                        if(!(Match=( tolower(*ca++)==tolower(*cb++))))
                        break;
                    }
                    else
                        if(!(Match=(*ca++==*cb++)))
                            break;
                }
            }
        }
    }
}

int compx(l1,l2)
struct line *l1, *l2;
{
    char sbuf1[MAXLEN+1], sbuf2[MAXLEN+1];
    register char *c;
    register int i;

    for(i=0, c = l1->text; *c; c++)
        if( !isspace(*c) )
            sbuf1[i++] = (Icase) ? tolower(*c) : *c ;
    sbuf1[i] = '\0';

    for(i=0, c = l2->text; *c; c++)
        if( !isspace(*c) ) sbuf2[i++] = (Icase) ? tolower(*c) : *c ;
    sbuf2[i] = '\0';

    if(strcmp(sbuf1,sbuf2) == 0)
        return 1;

    return 0;
}

fnoteq() /* Find a mismatch */
{
    for(;;)
    {
        movecurr(&a,in0);
        movecurr(&b,in1);
        mark(&a);
        mark(&b);
    compl();

        if(Match && a.curr != NULL)
        lastmseq=a.curr->seqnum;  /* Remember the sequence number */

    if(Eof || !Match)
        break;
    }
}

findeq()
{
    char advanceb;
    advanceb=1;
    for(;;)
    {
        advanceb = Eof ? endfanchor(&a):!advanceb;

        if(advanceb)
            search(&a,in0,&b,in1);
        else
            search(&b,in1,&a,in0);

        if(Match)
            break;
    }
    report();
}
search(x,filex,y,filey) /* Look ahead one line on fanchor y and search */
struct fanchor *x;       /* for that line backtracking on fanchor x */
FILE *filex;
struct fanchor *y;
FILE *filey;
{
    int count;  /* Number of lines backtraced on x */

    movecurr(y,filey);
    backtrack(x,&count);
    checkfullmatch(x,filex,y,filey);
    count--;
    while( count!=0 && !Match )
    {
        movecurr(x,filex);
        count--;
        checkfullmatch(x,filex,y,filey);
    }
}
checkfullmatch(      /* From the current positions of x and y, which may */
    x,filex,y,filey) /* match makesure that the next MINMATCH-1 lines */
struct fanchor *x;   /* also match or set match = false */
struct fanchor *y;
FILE *filex;
FILE *filey;
{
    int n;
    struct line *savexcur, *saveycur;
    int savexline, saveyline;

    savexcur=x->curr;
    saveycur=y->curr;
    savexline=x->cline;
    saveyline=y->cline;
    compl();
    for(n=MINMATCH-1; Match && n!=0;n--)
    {
        movecurr(x,filex);
        movecurr(y,filey);
        compl();
    }
    x->curr=savexcur;
    x->cline=savexline;
    y->curr=saveycur;
    y->cline=saveyline;
}

report()
{
#define REPLACE 0
#define DELETE 1
#define INSERT 2
#define INSERTOF 3

    int STATE;
    int lineno;
    struct line *p;
    struct line *alast; /* Pointer to the last unmatched lines in file a */
    struct line *blast; /* and file b */
    long seq1;
    long seq2;
    long seq3;
    long seq4;

    /* The following conditions are: */
    /* a.head = first mismatched line in file 1 */
    /* a.curr = first matched line in file 1 */
    /* b.head = first mismatched line in file 2 */
    /* b.curr = first matched line in file 2 */
    alast=blast=NULL;
    for(p=a.head;p!=a.curr && p!=NULL; p=p->next)
        alast=p;

    for(p=b.head;p!=b.curr && p!=NULL; p=p->next)
        blast=p;

    STATE=REPLACE;

    if(alast==NULL)
        STATE=INSERT;
    else
        if(blast==NULL)
            STATE=DELETE;

    if(STATE==INSERT && a.curr==NULL)
        STATE=INSERTOF;

    if(!Edit)
    {
        switch(STATE)
        {
            case REPLACE:
                pagechk();
                fprintf(outlist,"\nMismatch: file A, lines %ld to %ld not equal to file B, lines %ld to %ld",a.hline,a.cline - 1,b.hline,b.cline - 1);
                break;

            case INSERTOF:
                pagechk();
                fprintf(outlist,"\nMismatch: extra text on file B");
                break;

            case INSERT:
                pagechk();
                fprintf(outlist,"\nMismatch: inserted text before line %ld in file A",a.hline);
                break;

            case DELETE:
                pagechk();
                fprintf(outlist,"\nMismatch: lines %ld to %ld deleted from file A",a.hline,a.cline - 1,b.hline,b.cline - 1);
                break;

            default:
                break;
        }

        pagechk();
        fputc('\n',outlist);
        if(a.head!=a.curr)
        for(lineno=a.hline,p=a.head;p!=NULL && p!=a.curr;lineno++,p=p->next)
        {
            pagechk();
            fprintf(outlist," A %4d> %s",lineno,p->text);
            }
        if(b.head!=b.curr)
            for(lineno=b.hline,p=b.head;p!=NULL && p!=b.curr;lineno++,p=p->next)
            {
                pagechk();
                fprintf(outlist," B %4d> %s",lineno,p->text);
            }
    }
    else    /* Edit true */
    {  /* Handle an Update deck */

        switch(STATE)
        {
            case REPLACE:
                seq1=a.head->seqnum; /* Get the first sequence number */

                fprintf(outlist,"./ R %ld",seq1);

                if(a.head!=alast)
                {
                    seq2=alast->seqnum;
                    fprintf(outlist," %ld",seq2);
                }
                else
                    seq2=seq1;
                if(Seqtype!=GenSeq)
                {
                    seq4=b.cline-b.hline;  /* Get the number of lines inserted */
                    seq2=(a.curr!=NULL)?a.curr->seqnum:seq1+(seq4*seqincr);
                    seq4=(seq2-seq1)/(seq4+1L);
                    seq3=seq1+seq4;
                    fprintf(outlist," $ %ld %ld",seq3,seq4);
                }
                fputc('\n',outlist);
                for(p=b.head;p!=NULL && p!=b.curr;p=p->next)
                fprintf(outlist,"%s",p->text);
                break;

            case INSERTOF:
                seq1=a.curr->seqnum; /* Get the first sequence number */
                fprintf(outlist,"./ R %ld",seq1);
                seq2=seq1;
                if(Seqtype!=GenSeq)
                {
                    seq4=b.cline-b.hline+1;  /* Get the number of lines inserted */
                    p=a.curr->next;
                    seq2=(p!=NULL)?p->seqnum:seq1+(seq4*seqincr);
                    seq4=(p!=NULL)?(seq2-seq1)/(seq4+1L):seqincr;
                    seq3=seq1+seq4;
                    fprintf(outlist," $ %ld %ld",seq3,seq4);
                }
                fputc('\n',outlist);
                for(p=b.head;(p!=NULL && p!=b.curr);p=p->next)
                    fprintf(outlist,"%s",p->text);
                if(p==b.curr)
                    fprintf(outlist,"%s",p->text);
                break;

            case INSERT:
                fprintf(outlist,"./ I %ld",lastmseq);
                if(Seqtype!=GenSeq)
                {
                    seq1=b.cline-b.hline; /* Number of lines inserted */
                    p=a.curr->next;
                    seq2=(p!=NULL)?a.curr->seqnum:seqincr;
                    seq2=(p!=NULL)?(seq2-lastmseq)/(seq1+1L):seqincr;

                    seq1=lastmseq+seq2;
                    fprintf(outlist," $ %ld %ld",seq1,seq2);
                }
                fputc('\n',outlist);
                for(p=b.head;(p!=NULL && p!=b.curr);p=p->next)
                fprintf(outlist,"%s",p->text);
                break;

            case DELETE:
                fprintf(outlist,"./ D %ld",a.head->seqnum);
                if(alast!=NULL)
                    fprintf(outlist," %ld",alast->seqnum);
                fputc('\n',outlist);
                break;

            default:
                break;
        }
    }
}

initialize()
{
    initfanchor(&a,in0);
    initfanchor(&b,in1);
    Eof=a.eof || b.eof;
    templine.len=MAXLEN;
    templine.text[0]='\0';

    fputc('\n',outlist);
    fputs(VERSION,outlist);
    fprintf(outlist,"\n Comparing File A:<%s> with File B:<%s>\n\n",files[0],files[1]);

}

initfanchor(x,filex)
struct fanchor *x;
FILE *filex;
{
    x->curr=NULL;
    x->head=NULL;
    x->tail=NULL;
    x->cline=0;
    x->hline=0;
    x->tline=0;
    x->eof=feof(filex);
}

cmndformat()
{
    fputs("Command format:\n",console);
    fputs("COMPARE <file1> <file2> [options]\n",console);
    fputs("Where:\n",console);
    fputs(" <file1> and <file2> are the two files to be compared\n",console);
    fputs(" [options] are:\n",console);
    fputs(" -l [<listing>]  Generate output to file\n",console);
    fputs("                 <listing> defaults to COMPARE.LST\n",console);
    fputs(" -t              Ignore case\n",console);
    fputs(" -w              Ignore white space between 'words' in a line\n",console);
    fputs(" -p              Generate output to the printer\n",console);
    fputs(" -mn             Resync on n lines (default is 4)\n",console);
    fputs(" -cx,y           Only compare cols x thru y\n",console);
    fputs(" ------------ following for update deck generation only ------------\n",console);
    fputs(" -u [<update deck>] Generate update deck to make file1 into file2\n",console);
    fputs("                 <update deck> defaults to the <name of file1>.\"UPT\"\n",console);
    fputs(" -sx,y           Generate internal sequence numbers starting with x\n",console);
    fputs("                 and increment by y. Default if -s is omitted is 1,1\n",console);
    fputs("                 if -s is specified with out x and y then x=10000\n",console);
    fputs("                 y defaults to x\n",console);
    fputs(" -x              File1 already contains sequence numbers\n",console);
    fputs("------------ following for aide with editor shells only -----------\n",console);
    fputs(" -i              Use a sequence file (generated from UPDATE command)\n",console);
    fputs("                 name is <name of file1>.\"SEQ\"\n",console);
    fputs("                 will be deleted\n",console);
}

pagechk() /* Check to see if we are to do a page eject if we are printing */
{
    if(ToAFile||Print)
    {
        if(curline++ < LINESPERPAGE)
            return 0;       /* added 0 to prevent warning */
        fputc('\n',outlist);
        fputc('\f',outlist);
        fputs(VERSION,outlist);
        fprintf(outlist,"\n Comparing File A:<%s> with File B:<%s>\n\n",files[0],files[1]);
        curline=5;
    }
}

newext(fbuff,filename,newext)
char *fbuff;
char *filename;
char *newext;
{
   register char *in, *out;
   in = filename;
   out = fbuff;
   while ( *in && *in != '.' )
       *out++ = *in++;
   *out++ = '.';
   in = newext;
   while ( *in )
       *out++ = *in++;
   return 0;  /* added 0 to prevent warning */
}

