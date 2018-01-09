/*
 *  SimInf, a framework for stochastic disease spread simulations
 *  Copyright (C) 2015 Pavol Bauer
 *  Copyright (C) 2017 - 2018 Robin Eriksson
 *  Copyright (C) 2015 - 2018 Stefan Engblom
 *  Copyright (C) 2015 - 2018 Stefan Widgren
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <math.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include "SimInf.h"
#include "SimInf_solver.h"

/**
 * Sample individuals from a node
 *
 * Individuals are sampled from the states determined by select.
 *
 * @param irE Select matrix for events. irE[k] is the row of E[k].
 * @param jcE Select matrix for events. Index to data of first
 *        non-zero element in row k.
 * @param Nc Number of compartments in each node.
 * @param u The state vector with number of individuals in each
 *        compartment at each node. The current state in each node is
 *        offset by node * Nc.
 * @param node The node to sample.
 * @param select Column j in the Select matrix that determines the
 *        states to sample from.
 * @param n The number of individuals to sample. n >= 0.
 * @param proportion If n equals zero, then the number of individuals
 *        to sample is calculated by summing the number of individuals
 *        in the states determined by select and multiplying with the
 *        proportion. 0 <= proportion <= 1.
 * @param individuals The result of the sampling is stored in the
 *        individuals vector.
 * @param u_tmp Help vector for sampling individuals.
 * @param rng Random number generator.
 * @return 0 if Ok, else error code.
 */
int SimInf_sample_select(
    const int *irE, const int *jcE, int Nc, const int *u,
    int node, int select, int n, double proportion,
    int *individuals, int *u_tmp, gsl_rng *rng)
{
    int i, Nstates, Nindividuals = 0, Nkinds = 0;

    /* Clear vector with number of sampled individuals */
    memset(individuals, 0, Nc * sizeof(int));

    /* 1) Count number of states with individuals */
    /* 2) Count total number of individuals       */
    for (i = jcE[select]; i < jcE[select + 1]; i++) {
        int nk = u[node * Nc + irE[i]];
        if (nk > 0)
            Nkinds++;
        Nindividuals += nk;
    }

    /* Number of states */
    Nstates = jcE[select + 1] - jcE[select];

    /* If n == 0, use the proportion of Nindividuals, else use n as */
    /* the number of individuals to sample                          */
    if (n == 0)
        n = round(proportion * Nindividuals);

    /* Error checking. */
    if (Nstates <= 0 ||     /* No states to sample from, we shouldn't be here. */
        n > Nindividuals || /* Can not sample this number of individuals       */
        n < 0)              /* Can not sample negative number of individuals.  */
        return SIMINF_ERR_SAMPLE_SELECT;

    /* Handle cases that require no random sampling */
    if (n == 0) {
        /* We are done */
        return 0;
    } else if (Nindividuals == n) {
        /* Include all individuals */
        for (i = jcE[select]; i < jcE[select + 1]; i++)
            individuals[irE[i]] = u[node * Nc + irE[i]];
        return 0;
    } else if (Nstates == 1) {
        /* Only individuals from one state to select from. */
        individuals[irE[jcE[select]]] = n;
        return 0;
    } else if (Nkinds == 1) {
        /* All individuals to choose from in one state */
        for (i = jcE[select]; i < jcE[select + 1]; i++) {
            if (u[node * Nc + irE[i]] > 0) {
                individuals[irE[i]] = n;
                break;
            }
        }
        return 0;
    }

    /* Handle cases that require random sampling */
    if (Nstates == 2) {
        /* Sample from the hypergeometric distribution */
        i = jcE[select];
        individuals[irE[i]] = gsl_ran_hypergeometric(
            rng,
            u[node * Nc + irE[i]],
            u[node * Nc + irE[i+1]],
            n);
        individuals[irE[i+1]] = n - individuals[irE[i]];
    } else {
        /* Randomly sample n individuals from Nindividudals in
         * the Nstates */
        memcpy(u_tmp, &u[node * Nc], Nc * sizeof(int));
        while (n > 0) {
            double cum, rand = gsl_rng_uniform_pos(rng) * Nindividuals;

            /* Determine from which compartment the individual was
             * sampled from */
            for (i = jcE[select], cum = u_tmp[irE[i]];
                 i < jcE[select + 1] && rand > cum;
                 i++, cum += u_tmp[irE[i]]);

            /* Update sampled individual */
            u_tmp[irE[i]]--;
            individuals[irE[i]]++;

            Nindividuals--;
            n--;
        }
    }

    return 0;
}

/**
 * Allocate memory for scheduled events
 *
 * @param e scheduled_events structure for events.
 * @param n Number of events.
 * @return 0 on success else SIMINF_ERR_ALLOC_MEMORY_BUFFER
 */
int SimInf_allocate_events(SimInf_scheduled_events *e, int n)
{
    if (e && n > 0) {
        e->len = n;
        e->event = malloc(n * sizeof(int));
        if (!e->event)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        e->time = malloc(n * sizeof(int));
        if (!e->time)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        e->node = malloc(n * sizeof(int));
        if (!e->node)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        e->dest = malloc(n * sizeof(int));
        if (!e->dest)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        e->n = malloc(n * sizeof(int));
        if (!e->n)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        e->proportion = malloc(n * sizeof(double));
        if (!e->proportion)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        e->select = malloc(n * sizeof(int));
        if (!e->select)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        e->shift = malloc(n * sizeof(int));
        if (!e->shift)
            return SIMINF_ERR_ALLOC_MEMORY_BUFFER;
    }

    return 0;
}

/**
 * Free allocated memory for scheduled events
 *
 * @param e SimInf_scheduled_events to free
 */
static void SimInf_free_events(SimInf_scheduled_events *e)
{
    if (e) {
        if (e->event)
            free(e->event);
        e->event = NULL;
        if (e->time)
            free(e->time);
        e->time = NULL;
        if (e->node)
            free(e->node);
        e->node = NULL;
        if (e->dest)
            free(e->dest);
        e->dest = NULL;
        if (e->n)
            free(e->n);
        e->n = NULL;
        if (e->proportion)
            free(e->proportion);
        e->proportion = NULL;
        if (e->select)
            free(e->select);
        e->select = NULL;
        if (e->shift)
            free(e->shift);
        e->shift = NULL;
        free(e);
    }
}

/**
 * Free allocated memory to process events
 *
 * @param e SimInf_model_events to free
 */
void SimInf_free_model_events(SimInf_model_events *e)
{
    if (e) {
        if (e->E1)
            SimInf_free_events(e->E1);
        e->E1 = NULL;
        if (e->E2)
            SimInf_free_events(e->E2);
        e->E2 = NULL;
        if (e->individuals)
            free(e->individuals);
        e->individuals = NULL;
        if (e->u_tmp)
            free(e->u_tmp);
        e->u_tmp = NULL;
        if (e->rng)
            gsl_rng_free(e->rng);
        e->rng = NULL;
    }
}

/**
 * Free allocated memory to siminf thread arguments
 */
void SimInf_free_args(SimInf_thread_args *sa)
{
    if (sa) {
        if (sa->rng)
            gsl_rng_free(sa->rng);
        sa->rng = NULL;
        if (sa->t_rate)
            free(sa->t_rate);
        sa->t_rate = NULL;
        if (sa->sum_t_rate)
            free(sa->sum_t_rate);
        sa->sum_t_rate = NULL;
        if (sa->t_time)
            free(sa->t_time);
        sa->t_time = NULL;
        if (sa->individuals)
            free(sa->individuals);
        sa->individuals = NULL;
        if (sa->u_tmp)
            free(sa->u_tmp);
        sa->u_tmp = NULL;
        if (sa->E1)
            SimInf_free_events(sa->E1);
        sa->E1 = NULL;
        if (sa->E2)
            SimInf_free_events(sa->E2);
        sa->E2 = NULL;
        /* AEM variables */
	if(sa->rng_vec){
            for(int i = 0; i < (sa->Nn)*(sa->Nt); i++)
                gsl_rng_free(sa->rng_vec[i]);
        }
        sa->rng_vec = NULL;
        if(sa->reactHeap)
            free(sa->reactHeap);
        sa->reactHeap = NULL;
        if(sa->reactInf)
            free(sa->reactInf);
        sa->reactInf = NULL;
        if(sa->reactNode)
            free(sa->reactNode);
        sa->reactNode = NULL;
        if(sa->reactTimes)
            free(sa->reactTimes);
        sa->reactTimes = NULL;
    }
}

/**
 * Split scheduled events to E1 and E2 events by number of threads
 * used during simulation
 *
 * Thread id 0 is the main thread. All E2 events are assigned to
 * thread id 0.
 *
 * All E1 events for a node are assigned to the same thread.
 *
 * @param len Number of scheduled events.
 * @param event The type of event i.
 * @param time The time of event i.
 * @param node The source node index (one based) of event i.
 * @param dest The dest node index (one-based) of event i.
 * @param n The number of individuals in event i. n[i] >= 0.
 * @param proportion If n[i] equals zero, then the number of
 *        individuals to sample is calculated by summing the number of
 *        individuals in the states determined by select[i] and
 *        multiplying with the proportion. 0 <= p[i] <= 1.
 * @param select Column j (one-based) in the event matrix that
 *        determines the states to sample from.
 * @param shift Column j (one-based) in the shift matrix S that
 *        determines the shift of the internal and external
 *        transfer event.
 * @param Nn Total number of nodes.
 * @return 0 if Ok, else error code.
 */
int SimInf_split_events(
    SimInf_thread_args *sim_args,
    int len, const int *event, const int *time, const int *node,
    const int *dest, const int *n, const double *proportion,
    const int *select, const int *shift, int Nn, int Nthread)
{
    int i;
    int errcode = 0;
    int chunk_size = Nn / Nthread;
    int *E1_i = NULL;
    int E2_i = 0;

    /* Split events to each thread */
    E1_i = calloc(Nthread, sizeof(int));
    if (!E1_i) {
        errcode = SIMINF_ERR_ALLOC_MEMORY_BUFFER;
        goto cleanup;
    }

    for (i = 0; i < len; i++) {
        int k;

        switch (event[i]) {
        case EXIT_EVENT:
        case ENTER_EVENT:
        case INTERNAL_TRANSFER_EVENT:
            k = (node[i] - 1) / chunk_size;
            if (k >= Nthread)
                k = Nthread - 1;
            E1_i[k]++;
            break;
        case EXTERNAL_TRANSFER_EVENT:
            E2_i++;
            break;
        default:
            errcode = SIMINF_UNDEFINED_EVENT;
            goto cleanup;
        }
    }

    /* Allocate memory for E1 and E2 events. */
    for (i = 0; i < Nthread; i++) {
        errcode = SimInf_allocate_events(sim_args[i].E1, E1_i[i]);
        if (errcode)
            goto cleanup;
        E1_i[i] = 0;

        if (i == 0) {
            errcode = SimInf_allocate_events(sim_args[0].E2, E2_i);
            if (errcode)
                goto cleanup;
            E2_i = 0;
        }
    }

    for (i = 0; i < len; i++) {
        int j, k;
        SimInf_scheduled_events *e;

        switch (event[i]) {
        case EXIT_EVENT:
        case ENTER_EVENT:
        case INTERNAL_TRANSFER_EVENT:
            k = (node[i] - 1) / chunk_size;
            if (k >= Nthread)
                k = Nthread - 1;
            j = E1_i[k]++;
            e = sim_args[k].E1;
            break;
        case EXTERNAL_TRANSFER_EVENT:
            j = E2_i++;
            e = sim_args[0].E2;
            break;
        default:
            errcode = SIMINF_UNDEFINED_EVENT;
            goto cleanup;
        }

        e->event[j]      = event[i];
        e->time[j]       = time[i];
        e->node[j]       = node[i] - 1;
        e->dest[j]       = dest[i] - 1;
        e->n[j]          = n[i];
        e->proportion[j] = proportion[i];
        e->select[j]     = select[i] - 1;
        e->shift[j]      = shift[i] - 1;
    }

cleanup:
    if (E1_i)
        free(E1_i);

    return errcode;
}

void SimInf_process_E1_events(SimInf_thread_args *sim_args, int *uu, int *update_node)
{
    SimInf_thread_args sa = *&sim_args[0];
    SimInf_scheduled_events e1 = *sa.E1;

    while (sa.E1_index < e1.len &&
           sa.tt >= e1.time[sa.E1_index] &&
           !sa.errcode)
    {
        const int j = sa.E1_index;
        const int s = e1.select[j];

        if (e1.event[j] == ENTER_EVENT) {
            /* All individuals enter first non-zero compartment,
             * i.e. a non-zero entry in element in the select
             * column. */
            if (sa.jcE[s] < sa.jcE[s + 1]) {
                uu[e1.node[j] * sa.Nc + sa.irE[sa.jcE[s]]] += e1.n[j];
                if (uu[e1.node[j] * sa.Nc + sa.irE[sa.jcE[s]]] < 0)
                    sa.errcode = SIMINF_ERR_NEGATIVE_STATE;
            }
        } else {
            sa.errcode = SimInf_sample_select(
                sa.irE, sa.jcE, sa.Nc, uu, e1.node[j],
                e1.select[j], e1.n[j], e1.proportion[j],
                sa.individuals, sa.u_tmp, sa.rng);

            if (sa.errcode)
                break;

            if (e1.event[j] == EXIT_EVENT) {
                int ii;

                for (ii = sa.jcE[s]; ii < sa.jcE[s + 1]; ii++) {
                    const int jj = sa.irE[ii];
                    const int kk = e1.node[j] * sa.Nc + jj;

                    /* Remove individuals from node */
                    uu[kk] -= sa.individuals[jj];
                    if (uu[kk] < 0) {
                        sa.errcode = SIMINF_ERR_NEGATIVE_STATE;
                        break;
                    }
                }
            } else { /* INTERNAL_TRANSFER_EVENT */
                int ii;

                for (ii = sa.jcE[s]; ii < sa.jcE[s + 1]; ii++) {
                    const int jj = sa.irE[ii];
                    const int kk = e1.node[j] * sa.Nc + jj;
                    const int ll = sa.N[e1.shift[j] * sa.Nc + jj];

                    /* Add individuals to new compartments in node */
                    uu[kk + ll] += sa.individuals[jj];
                    if (uu[kk + ll] < 0) {
                        sa.errcode = SIMINF_ERR_NEGATIVE_STATE;
                        break;
                    }

                    /* Remove individuals from previous compartments
                     * in node */
                    uu[kk] -= sa.individuals[jj];
                    if (uu[kk] < 0) {
                        sa.errcode = SIMINF_ERR_NEGATIVE_STATE;
                        break;
                    }
                }
            }
        }

        /* Indicate node for update */
        update_node[e1.node[j]] = 1;
        sa.E1_index++;
    }

    *&sim_args[0] = sa;
}

void SimInf_process_E2_events(SimInf_thread_args *sim_args, int *uu, int *update_node)
{
    SimInf_thread_args sa = *&sim_args[0];
    SimInf_scheduled_events e2 = *sa.E2;

    /* (3) Incorporate all scheduled E2 events */
    while (sa.E2_index < e2.len &&
           sa.tt >= e2.time[sa.E2_index] &&
           !sa.errcode)
    {
        int i;

        sa.errcode = SimInf_sample_select(
            sa.irE, sa.jcE, sa.Nc, uu, e2.node[sa.E2_index],
            e2.select[sa.E2_index], e2.n[sa.E2_index],
            e2.proportion[sa.E2_index], sa.individuals,
            sa.u_tmp, sa.rng);

        if (sa.errcode)
            break;

        for (i = sa.jcE[e2.select[sa.E2_index]];
             i < sa.jcE[e2.select[sa.E2_index] + 1];
             i++)
        {
            const int jj = sa.irE[i];
            const int k1 = e2.dest[sa.E2_index] * sa.Nc + jj;
            const int k2 = e2.node[sa.E2_index] * sa.Nc + jj;
            const int ll = e2.shift[sa.E2_index] < 0 ? 0 :
                sa.N[e2.shift[sa.E2_index] * sa.Nc + jj];

            /* Add individuals to dest */
            uu[k1 + ll] += sa.individuals[jj];
            if (uu[k1 + ll] < 0) {
                sa.errcode = SIMINF_ERR_NEGATIVE_STATE;
                break;
            }

            /* Remove individuals from node */
            uu[k2] -= sa.individuals[jj];
            if (uu[k2] < 0) {
                sa.errcode = SIMINF_ERR_NEGATIVE_STATE;
                break;
            }
        }

        /* Indicate node and dest for update */
        update_node[e2.node[sa.E2_index]] = 1;
        update_node[e2.dest[sa.E2_index]] = 1;
        sa.E2_index++;
    }

    *&sim_args[0] = sa;
}

/**
 * Handle the case where the solution is stored in a sparse matrix
 *
 * Store solution if tt has passed the next time in tspan. Report
 * solution up to, but not including tt.
 *
 * @param SimInf_thread_args *sim_args Data structure with thread
 *        specific data/arguments for simulation.
 */
void SimInf_store_solution_sparse(SimInf_thread_args *sim_args)
{
    while (!sim_args[0].U && sim_args[0].U_it < sim_args[0].tlen &&
           sim_args[0].tt > sim_args[0].tspan[sim_args[0].U_it]) {
        int j;

        /* Copy compartment state to U_sparse */
        for (j = sim_args[0].jcU[sim_args[0].U_it];
             j < sim_args[0].jcU[sim_args[0].U_it + 1]; j++)
            sim_args[0].prU[j] = sim_args[0].u[sim_args[0].irU[j]];
        sim_args[0].U_it++;
    }

    while (!sim_args[0].V && sim_args[0].V_it < sim_args[0].tlen &&
           sim_args[0].tt > sim_args[0].tspan[sim_args[0].V_it]) {
        int j;

        /* Copy continuous state to V_sparse */
        for (j = sim_args[0].jcV[sim_args[0].V_it];
             j < sim_args[0].jcV[sim_args[0].V_it + 1]; j++)
            sim_args[0].prV[j] = sim_args[0].v_new[sim_args[0].irV[j]];
        sim_args[0].V_it++;
    }
}
