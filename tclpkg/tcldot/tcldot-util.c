/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: See CVS logs. Details at http://www.graphviz.org/
 *************************************************************************/


#include "tcldot.h"

size_t Tcldot_string_writer(GVJ_t *job, const char *s, size_t len)
{
    Tcl_AppendResult((Tcl_Interp*)(job->context), s, NULL);
    return len;
}

size_t Tcldot_channel_writer(GVJ_t *job, const char *s, size_t len)
{
    return Tcl_Write((Tcl_Channel)(job->output_file), s, len);
}

void reset_layout(GVC_t *gvc, Agraph_t * sg)
{
    Agraph_t *g = agroot(sg);

    if (GD_drawing(g)) {	/* only cleanup once between layouts */
	gvFreeLayout(gvc, g);
	GD_drawing(g) = NULL;
    }
}

#ifdef WITH_CGRAPH
/* handles (tcl commands) to obj* */

Agraph_t *cmd2g(gctx_t *gctx, char *cmd) {
    Agraph_t *g = NULL;

    if (sscanf(cmd, "graph%p", &g) != 1 || !g)
        return NULL;
    return g;
}
Agnode_t *cmd2n(gctx_t *gctx, char *cmd) {
    Agnode_t *n = NULL;

    if (sscanf(cmd, "node%p", &n) != 1 || !n)
        return NULL;
    return n;
}
Agedge_t *cmd2e(gctx_t *gctx, char *cmd) {
    Agedge_t *e = NULL;

    if (sscanf(cmd, "edge%p", &e) != 1 || !e)
        return NULL;
    return e;
}
#endif // WITH_CGRAPH

#ifdef WITH_CGRAPH
void deleteEdge(gctx_t *gctx, Agraph_t * g, Agedge_t *e)
{
    char buf[32];

    sprintf(buf,"edge%p",e);
    agdelete(gctx->g, e);  /* delete edge from root graph */
    Tcl_DeleteCommand(gctx->ictx->interp, buf);
}
static void deleteNodeEdges(gctx_t *gctx, Agraph_t *g, Agnode_t *n)
{
    Agedge_t *e, *e1;

    e = agfstedge(g, n);
    while (e) {
	e1 = agnxtedge(g, e, n);
	deleteEdge(gctx, g, e);
	e = e1;
    }
}
void deleteNode(gctx_t * gctx, Agraph_t *g, Agnode_t *n)
{
    char buf[32];

    deleteNodeEdges(gctx, gctx->g, n); /* delete all edges to/from node in root graph */

    sprintf(buf,"node%p",n);
    agdelete(gctx->g, n); /* delete node from root graph */
    Tcl_DeleteCommand(gctx->ictx->interp, buf);
}
static void deleteGraphNodes(gctx_t * gctx, Agraph_t *g)
{
    Agnode_t *n, *n1;

    n = agfstnode(g);
    while (n) {
	n1 = agnxtnode(g, n);
	deleteNode(gctx, g, n);
	n = n1;
    }
}
void deleteGraph(gctx_t * gctx, Agraph_t *g)
{
    Agraph_t *sg;
    char buf[32];

    for (sg = agfstsubg (g); sg; sg = agnxtsubg (sg)) {
	deleteGraph(gctx, sg);
    }
    deleteGraphNodes(gctx, g);

    sprintf(buf,"graph%p",g);
    if (g == agroot(g)) {
	agclose(g);
    } else {
	agdelsubg(agroot(g), g);
    }
    Tcl_DeleteCommand(gctx->ictx->interp, buf);
}
#else
void deleteEdge(ictx_t * ictx, Agraph_t * g, Agedge_t *e)
{
    Agedge_t **ep;
    char buf[32];

    tclhandleString(ictx->edgeTblPtr, buf, AGID(e));
    Tcl_DeleteCommand(ictx->interp, buf);
    ep = (Agedge_t **) tclhandleXlateIndex(ictx->edgeTblPtr, AGID(e));
    if (!ep)
	fprintf(stderr, "Bad entry in edgeTbl\n");
    tclhandleFreeIndex(ictx->edgeTblPtr, AGID(e));
    agdelete(agroot(g), e);
}
static void deleteNodeEdges(ictx_t * ictx, Agraph_t * g, Agnode_t * n)
{
    Agedge_t *e, *e1;

    e = agfstedge(g, n);
    while (e) {
	e1 = agnxtedge(g, e, n);
        deleteEdge(ictx, g, e);
	e = e1;
    }
}
void deleteNode(ictx_t * ictx, Agraph_t * g, Agnode_t *n)
{
    Agnode_t **np;
    char buf[32];
    
    deleteNodeEdges(ictx, agroot(g), n);
    tclhandleString(ictx->nodeTblPtr, buf, AGID(n));
    Tcl_DeleteCommand(ictx->interp, buf);
    np = (Agnode_t **) tclhandleXlateIndex(ictx->nodeTblPtr, AGID(n));
    if (!np)
	fprintf(stderr, "Bad entry in nodeTbl\n");
    tclhandleFreeIndex(ictx->nodeTblPtr, AGID(n));
    agdelete(agroot(g), n);
}
static void deleteGraphNodes(ictx_t * ictx, Agraph_t * g)
{
    Agnode_t *n, *n1;

    n = agfstnode(g);
    while (n) {
	n1 = agnxtnode(g, n);
	deleteNode(ictx, g, n);
	n = n1;
    }
}
void deleteGraph(ictx_t * ictx, Agraph_t * g)
{
    Agraph_t **sgp;
    Agedge_t *e;
    char buf[32];

    if (g->meta_node) {
	for (e = agfstout(g->meta_node->graph, g->meta_node); e;
	     e = agnxtout(g->meta_node->graph, e)) {
	    deleteGraph(ictx, agusergraph(aghead(e)));
	}
	tclhandleString(ictx->graphTblPtr, buf, AGID(g));
	Tcl_DeleteCommand(ictx->interp, buf);
	sgp = (Agraph_t **) tclhandleXlateIndex(ictx->graphTblPtr, AGID(g));
	if (!sgp)
	    fprintf(stderr, "Bad entry in graphTbl\n");
	tclhandleFreeIndex(ictx->graphTblPtr, AGID(g));
	deleteGraphNodes(ictx, g);
	if (g == agroot(g)) {
	    agclose(g);
	} else {
	    agdelete(g->meta_node->graph, g->meta_node);
	}
    } else {
	fprintf(stderr, "Subgraph has no meta_node\n");
    }
}
#endif

void setgraphattributes(Agraph_t * g, char *argv[], int argc)
{
    int i;
    Agsym_t *a;

    for (i = 0; i < argc; i++) {
	if (!(a = agfindgraphattr(agroot(g), argv[i])))
#ifndef WITH_CGRAPH
	    a = agraphattr(agroot(g), argv[i], "");
	agxset(g, a->index, argv[++i]);
#else
	    a = agattr(agroot(g), AGRAPH, argv[i], "");
	agxset(g, a, argv[++i]);
#endif
    }
}

void setedgeattributes(Agraph_t * g, Agedge_t * e, char *argv[], int argc)
{
    int i;
    Agsym_t *a;

    for (i = 0; i < argc; i++) {
	/* silently ignore attempts to modify "key" */
	if (strcmp(argv[i], "key") == 0) {
	    i++;
	    continue;
	}
	if (!(a = agfindedgeattr(g, argv[i])))
#ifndef WITH_CGRAPH
	    a = agedgeattr(agroot(g), argv[i], "");
	agxset(e, a->index, argv[++i]);
#else
	    a = agattr(agroot(g), AGEDGE, argv[i], "");
	agxset(e, a, argv[++i]);
#endif
    }
}

void setnodeattributes(Agraph_t * g, Agnode_t * n, char *argv[], int argc)
{
    int i;
    Agsym_t *a;

    for (i = 0; i < argc; i++) {
	if (!(a = agfindnodeattr(g, argv[i])))
#ifndef WITH_CGRAPH
	    a = agnodeattr(agroot(g), argv[i], "");
	agxset(n, a->index, argv[++i]);
#else
	    a = agattr(agroot(g), AGNODE, argv[i], "");
	agxset(n, a, argv[++i]);
#endif
    }
}

#ifdef WITH_CGRAPH
void listGraphAttrs (Tcl_Interp * interp, Agraph_t* g)
{
    Agsym_t *a = NULL;
    while ((a = agnxtattr(g, AGRAPH, a))) {
	Tcl_AppendElement(interp, a->name);
    }
}
void listNodeAttrs (Tcl_Interp * interp, Agraph_t* g)
{
    Agsym_t *a = NULL;
    while ((a = agnxtattr(g, AGNODE, a))) {
	Tcl_AppendElement(interp, a->name);
    }
}
void listEdgeAttrs (Tcl_Interp * interp, Agraph_t* g)
{
    Agsym_t *a = NULL;
    while ((a = agnxtattr(g, AGEDGE, a))) {
	Tcl_AppendElement(interp, a->name);
    }
}
#else
void listGraphAttrs (Tcl_Interp * interp, Agraph_t* g)
{
    int i;
    Agsym_t *a;

    for (i = 0; i < dtsize(g->univ->globattr->dict); i++) {
	a = g->univ->globattr->list[i];
	Tcl_AppendElement(interp, a->name);
    }
}
void listNodeAttrs (Tcl_Interp * interp, Agraph_t* g)
{
    int i;
    Agsym_t *a;

    for (i = 0; i < dtsize(g->univ->nodeattr->dict); i++) {
	a = g->univ->nodeattr->list[i];
	Tcl_AppendElement(interp, a->name);
    }
}
void listEdgeAttrs (Tcl_Interp * interp, Agraph_t* g)
{
    int i;
    Agsym_t *a;

    for (i = 0; i < dtsize(g->univ->edgeattr->dict); i++) {
	a = g->univ->edgeattr->list[i];
	Tcl_AppendElement(interp, a->name);
    }
}
#endif

void tcldot_layout(GVC_t *gvc, Agraph_t * g, char *engine)
{
    char buf[256];
    Agsym_t *a;
    int rc;

    reset_layout(gvc, g);		/* in case previously drawn */

/* support old behaviors if engine isn't specified*/
    if (!engine || *engine == '\0') {
	if (agisdirected(g))
	    rc = gvlayout_select(gvc, "dot");
	else
	    rc = gvlayout_select(gvc, "neato");
    }
    else {
	if (strcasecmp(engine, "nop") == 0) {
	    Nop = 2;
	    PSinputscale = POINTS_PER_INCH;
	    rc = gvlayout_select(gvc, "neato");
	}
	else {
	    rc = gvlayout_select(gvc, engine);
	}
	if (rc == NO_SUPPORT)
	    rc = gvlayout_select(gvc, "dot");
    }
    if (rc == NO_SUPPORT) {
        fprintf(stderr, "Layout type: \"%s\" not recognized. Use one of:%s\n",
                engine, gvplugin_list(gvc, API_layout, engine));
        return;
    }
    gvLayoutJobs(gvc, g);

/* set bb attribute for basic layout.
 * doesn't yet include margins, scaling or page sizes because
 * those depend on the renderer being used. */
    if (GD_drawing(g)->landscape)
	sprintf(buf, "%d %d %d %d",
		ROUND(GD_bb(g).LL.y), ROUND(GD_bb(g).LL.x),
		ROUND(GD_bb(g).UR.y), ROUND(GD_bb(g).UR.x));
    else
	sprintf(buf, "%d %d %d %d",
		ROUND(GD_bb(g).LL.x), ROUND(GD_bb(g).LL.y),
		ROUND(GD_bb(g).UR.x), ROUND(GD_bb(g).UR.y));
#ifndef WITH_CGRAPH
    if (!(a = agfindgraphattr(g, "bb"))) 
	a = agraphattr(g, "bb", "");
    agxset(g, a->index, buf);
#else
    if (!(a = agattr(g, AGRAPH, "bb", NULL))) 
	a = agattr(g, AGRAPH, "bb", "");
    agxset(g, a, buf);
#endif
}
