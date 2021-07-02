/* TAILQ_* implementation macros. */

/* Declare a TAILQ_HEAD structure.
 * HEADNAME: Name of the structure to be defined.
 * TYPE: Type of the elements to be linked into the tail queue.
 */
#define TAILQ_HEAD(HEADNAME, TYPE) \
  struct HEADNAME { struct TYPE *first; struct TYPE **last; }

/* An initializer for a tail queue.
 * HEAD: Tail queue being initialized.
 */
#define TAILQ_HEAD_INITIALIZER(HEAD) { NULL, &(HEAD).first }

/* Declare a structure that connects the elements in the tail queue.
 * TYPE: Type of the elements linked into the tail queue.
 */
#define TAILQ_ENTRY(TYPE) struct { struct TYPE *next; struct TYPE **prev; }

/* True if there are no elements in the tail queue.
 * HEAD: Tail queue being checked.
 */
#define TAILQ_EMPTY(HEAD) (TAILQ_FIRST(HEAD) == NULL)

/* The first element in the tail queue or NULL if empty.
 * HEAD: Tail queue being checked.
 */
#define TAILQ_FIRST(HEAD) ((HEAD)->first)

/* The next element in the tail queue or NULL if this element is the last.
 * ELM: Element of the tail queue.
 * NAME: Name of the tail queue data member of the tail queue entry.
 */
#define TAILQ_NEXT(ELM, NAME) ((ELM)->NAME.next)

/* Traverse the tail queue in the forward direction, assigning each element in
 * turn to VAR. VAR is set to NULL if the loop completes normally or if there
 * were no elements. Within the loop, VAR cannot be removed from the tail queue
 * or modified.
 * VAR: Variable to hold each tail queue entry.
 * HEAD: Tail queue being traversed.
 * NAME: Name of the tail queue data member of the tail queue entry.
 */
#define TAILQ_FOREACH(VAR, HEAD, NAME) \
  for ((VAR) = TAILQ_FIRST(HEAD); \
       (VAR) != NULL; \
       (VAR) = TAILQ_NEXT(VAR, NAME))

/* Traverse the tail queue in the forward direction, assigning each element in
 * turn to VAR. VAR is set to NULL if the loop completes normally or if there
 * were no elements. Within the loop, VAR can be removed from the tail queue or
 * modified, but TEMP_VAR cannot.
 * VAR: Variable to hold each tail queue entry.
 * HEAD: Tail queue being traversed.
 * NAME: Name of the tail queue data member of the tail queue entry.
 * TEMP_VAR: Temporary variable to hold each tail queue entry.
 */
#define TAILQ_FOREACH_SAFE(VAR, HEAD, NAME, TEMP_VAR) \
  for ((VAR) = TAILQ_FIRST(HEAD), (TEMP_VAR) = (VAR); \
       (TEMP_VAR) != NULL && ((TEMP_VAR) = TAILQ_NEXT((TEMP_VAR), NAME), 1); \
       (VAR) = (TEMP_VAR))

/* Insert a new element at the end of the tail queue.
 * HEAD: Tail queue being modified.
 * ELM: Element to be added.
 * NAME: Name of the tail queue data member of the tail queue entry.
 */
#define TAILQ_INSERT_TAIL(HEAD, ELM, NAME) \
  do { \
    (ELM)->NAME.next = NULL; \
    (ELM)->NAME.prev = (HEAD)->last; \
    *(HEAD)->last = (ELM); \
    (HEAD)->last = &(ELM)->NAME.next; \
  } while (0)

/* Remove an element from the tail queue.
 * HEAD: Tail queue being modified.
 * ELM: Element to be removed.
 * NAME: Name of the tail queue data member of the tail queue entry.
 */
#define TAILQ_REMOVE(HEAD, ELM, NAME) \
  do { \
    if (((ELM)->NAME.next) != NULL) \
      (ELM)->NAME.next->NAME.prev = (ELM)->NAME.prev; \
    else \
      (HEAD)->last = (ELM)->NAME.prev; \
    *(ELM)->NAME.prev = (ELM)->NAME.next; \
    (ELM)->NAME.next = NULL; \
    (ELM)->NAME.prev = NULL; \
  } while (0)
