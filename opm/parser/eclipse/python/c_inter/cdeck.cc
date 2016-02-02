#include <iostream>
#include <opm/parser/eclipse/Deck/Deck.hpp>

extern "C" {

    int deck_size( const Opm::Deck * deck ) {
        return static_cast<int>( deck->size() );
    }


    Opm::Deck * deck_alloc() {
        return new Opm::Deck();
    }


    void deck_free( Opm::Deck * deck ) {
        delete deck;
    }


    const Opm::DeckKeyword * deck_iget_keyword( const Opm::Deck * deck , int index) {
        auto shared = deck->getKeyword( index );
        return shared.get();
    }


    const Opm::DeckKeyword * deck_iget_named_keyword( const Opm::Deck * deck , const char * keyword , int index) {
        auto shared = deck->getKeyword( keyword , index );
        return shared.get();
    }


    bool deck_has_keyword( const Opm::Deck * deck , const char * keyword) {
        return deck->hasKeyword( keyword );
    }

    int deck_num_keywords( const Opm::Deck * deck , const char * keyword) {
        return static_cast<int>( deck->numKeywords(keyword) );
    }


}

