#include "outbound_molecules.h"
#include "mem_util.h"
#include "mcell_structs.h"
#include "vol_util.h"
#include "logging.h"
#include <stdlib.h>

static transmitted_molecules_t *transmitted_molecules_add(transmitted_molecules_t *head,
                                                          struct volume_molecule *mol,
                                                          struct subvolume    *target,
                                                          struct vector3        *disp,
                                                          double              t_remain)
{
  if (head == NULL  ||
      head->fill == MOLECULE_QUEUE_LENGTH)
  {
    transmitted_molecules_t *new_mqueue = CHECKED_MALLOC_STRUCT(transmitted_molecules_t, "outbound molecule queue page");
    new_mqueue->next = head;
    new_mqueue->fill = 0;
    head = new_mqueue;
  }

  /* Store the molecule. */
  head->molecules[head->fill].molecule       = mol;
  head->molecules[head->fill].target         = target;
  head->molecules[head->fill].disp_remainder = *disp;
  head->molecules[head->fill].time_remainder = t_remain;
  ++ head->fill;
  return head;
}


void outbound_molecules_add_molecule(outbound_molecules_t *queue,
                                     struct volume_molecule *mol,
                                     struct subvolume    *target,
                                     struct vector3        *disp,
                                     double             t_remain)
{
  /* Get an empty page. */
  queue->molecule_queue = transmitted_molecules_add(queue->molecule_queue,
                                                    mol, target, disp,
                                                    t_remain);
}

void outbound_molecules_add_release(outbound_molecules_t *queue,
                                    struct release_event_queue *event,
                                    struct magic_list *incantation)
{
  /* Allocate a new "delayed release". */
  delayed_release_t *release = CHECKED_MALLOC_STRUCT(delayed_release_t, "delayed release");
  release->next         = queue->release_queue;
  release->event        = *event;
  release->incantation  = incantation;
}

void outbound_molecules_play(struct volume *world,
                             outbound_molecules_t *queue)
{
  /* Play back all delayed releases. */
  while (queue->release_queue != NULL)
  {
    delayed_release_t *cur = queue->release_queue;
    queue->release_queue = cur->next;

    /* Perform a reaction-triggered release. */
    for (struct magic_list *incantation = cur->incantation;
         incantation != NULL;
         incantation=incantation->next)
    {
      if (incantation->type != magic_release)
        continue;

      cur->event.release_site = (struct release_site_obj*)incantation->data;
      if (release_molecules(&cur->event))
        mcell_error("Failed to perform reaction-triggered release from site '%s'.",
                    cur->event.release_site->name);
    }

    free(cur);
  }

  /* Play back all molecule transfers. */
  while (queue->molecule_queue != NULL)
  {
    transmitted_molecules_t *cur = queue->molecule_queue;
    queue->molecule_queue = cur->next;

    /* Process all molecules in this page. */
    for (int i=0; i<cur->fill; ++i)
    {
      /* Place this molecule. */
      /* 1. Find the storage responsible for this molecule. */
      struct storage *stg = cur->molecules[i].target->local_storage;

      /* 2. Duplicate the molecule and free the old one. */
      struct volume_molecule *new_m = (struct volume_molecule *)
                  CHECKED_MEM_GET(stg->mol, "volume molecule");
      * new_m = * (cur->molecules[i].molecule);
      new_m->birthplace = stg->mol;
      new_m->prev_v = NULL;
      new_m->next_v = NULL;
      new_m->next = NULL;
      new_m->subvol = cur->molecules[i].target;
      mem_put(cur->molecules[i].molecule->birthplace,
              cur->molecules[i].molecule);

      /* 3. Toss it into the "incoming" queue. */
      stg->inbound = transmitted_molecules_add(stg->inbound,
                                               new_m,
                                               cur->molecules[i].target,
                                             & cur->molecules[i].disp_remainder,
                                               cur->molecules[i].time_remainder);

      /* 4. Update the molecules count for this subvolume. */
      ++ cur->molecules[i].target->mol_count;
    }

    free(cur);
  }
}

transmitted_molecule_t *outbound_molecules_next(outbound_molecules_t *queue,
                                                transmitted_molecule_iter_t *iter)
{
  /* Get the next bank of molecules. */
  transmitted_molecules_t *cur = queue->molecule_queue;
  if (cur == NULL  ||  *iter < 0)
  {
    *iter = -1;
    return NULL;
  }

  /* If our iterator is out-of-bounds, advance to the next page and retry. */
  if (*iter >= cur->fill)
  {
    *iter = 0;
    queue->molecule_queue = cur->next;
    free(cur);
    return outbound_molecules_next(queue, iter);
  }

  /* Grab the next molecule on this page. */
  return & cur->molecules[(*iter) ++];
}
