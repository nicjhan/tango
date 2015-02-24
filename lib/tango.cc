#include <list>
#include <assert.h>
#include <mpi.h>

#include "tango.h"
#include "router.h"

using namespace std;

class Field {
public:
    double *buffer;
    unsigned int size;
    Field(double *buf, unsigned int buf_size);
};

Field::Field(double *buf, unsigned int buf_size)
    : buffer(buf), size(buf_size) {}

class Transfer {
private:
    int curr_time;
    int sender;
    /* Name of grid that this transfer is sending/recieving to/from. */
    string peer_grid;
public:
    unsigned int total_send_size;
    unsigned int total_recv_size;
    string get_peer_grid(void) const { return peer_grid; }
    list<Field> fields;
    Transfer(int time, string peer);
};

Transfer::Transfer(int time, string peer)
    : curr_time(time), peer_grid(peer), total_send_size(0), total_recv_size(0) {}

static Transfer *transfer;
static CouplingManager *cm;

/* Pass in the grid name, the extents of the global domain and the extents of
 * the local domain that this proc is responsible for. */
void tango_init(const char *config, const char *grid_name,
                /* Local  domain */
                unsigned int lis, unsigned int lie,
                unsigned int ljs, unsigned int lje,
                /* Global domain */
                unsigned int gis, unsigned int gie,
                unsigned int gjs, unsigned int gje)
{
    assert(cm == nullptr);
    assert(transfer == nullptr);

    /* FIXME: what to do about Fortran indexing convention here. For the time
     * being stick to C++/Python. */

    /* Build the coupling manager for this process. This lasts for the lifetime
     * of the process. */
    cm = new CouplingManager(string(config), string(grid_name),
                             lis, lie, ljs, lje, gis, gie, gjs, gje);
}

void tango_begin_transfer(int time, const char* grid)
{
    assert(transfer == NULL);
    transfer = new Transfer(time, string(grid));
}

void tango_put(const char *field_name, double array[], int size)
{
    assert(transfer != NULL);
    assert(transfer->total_recv_size == 0);
    assert(cm->can_send_field_to_grid(string(field_name),
                                      transfer->get_peer_grid()));

    transfer->total_send_size += size;
    transfer->fields.push_back(Field(array, size));
}

void tango_get(const char *field_name, double array[], int size)
{
    assert(transfer != nullptr);
    assert(transfer->total_send_size == 0);
    assert(cm->can_recv_field_from_grid(string(field_name),
                                        transfer->get_peer_grid()));

    transfer->total_recv_size += size;
    transfer->fields.push_back(Field(array, size));
}

void tango_end_transfer()
{
    int sender;
    unsigned int offset;

    assert(transfer != nullptr);
    /* Check that this is either all send or all receive. */
    assert(transfer->total_send_size == 0 || transfer->total_recv_size == 0);
    assert(transfer->total_send_size != 0 || transfer->total_recv_size != 0);

    Router *router = cm->get_router();

    /* We are the sender */
    if (transfer->total_send_size != 0) {

        /* Iterate over the tiles we are sending to. */
        for (const auto *tile : router->get_dest_tiles(transfer->get_peer_grid())) {

            /* Which local points to send to each destination tile. */
            const auto& points = tile->get_send_points();
            const auto& weights = tile->get_weights();

            /* Marshall data into buffer for current send. All variables are
             * sent at once. */
            double send_buf[points.size() * transfer->fields.size()];
            offset = 0;
            for (const auto& field : transfer->fields) {
                for (int i = 0; i < points.size(); i++) {
                    /* Note that points is in the local coordinate system (not global) */
                    send_buf[offset] = field.buffer[points[i]] * weights[i];
                    offset++;
                }
            }

            /* Now do the actual send to the remote tile. */
            /*
            MPI_Isend();
            */
        }

    } else {

        /* We are the receiver. */
        /* Receive from source ranks. */
        /* Copy recv into individual field buffers. */
    }

    /* Transfer is complete. */
    delete(transfer);
}

void tango_finalize()
{
    assert(transfer == nullptr);
}
